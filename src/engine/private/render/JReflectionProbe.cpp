#include "engine/render/JReflectionProbe.h"

#include "engine/core/JEngineContext.h"
#include "engine/d3dx12.h"
#include "engine/platform/JDevice.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JCommandQueue.h"
#include "engine/render/JGraphicResource.h"
#include "engine/render/JRenderTarget.h"
#include "engine/asset/JShader.h"

#include <iostream>

J_ENGINE_BEGIN

namespace
{
	constexpr uint32 BRDF_LUT_SIZE = 256;
	constexpr uint32 CAPTURE_CUBE_SIZE = 256;
	constexpr uint32 IRRADIANCE_SIZE = 32;
	constexpr uint32 PREFILTER_SIZE = 128;
	constexpr uint32 PREFILTER_MIPS = 5;

	// irradiance 면 방향 복원에 쓰는 face basis 상수 버퍼.
	struct PerFaceConstants
	{
		JVec4 forward;
		JVec4 right;
		JVec4 up;
	};

	// prefilter용: face basis + roughness.
	struct PerPrefilterConstants
	{
		JVec4 forward;
		JVec4 right;
		JVec4 up;
		JVec4 params; // x = roughness
	};

	// D3D11/12 TextureCube 면 순서/방향(LH): +X,-X,+Y,-Y,+Z,-Z. (capture와 동일 규약)
	const JVec4 FACE_FORWARD[6] =
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f, 0.0f },
	};
	const JVec4 FACE_UP[6] =
	{
		{ 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f },
	};

	JVec4 crossVec(const JVec4& a, const JVec4& b)
	{
		return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x, 0.0f };
	}

	// 오프스크린 bake용 일회성 command context를 만들어 full-screen draw 한 번을 실행하고
	// 타겟을 PIXEL_SHADER_RESOURCE 상태로 전환한 뒤 GPU 완료까지 대기한다.
	// (로드 시 1회만 도므로 자체 queue/fence를 쓰는 게 main frame 흐름과 분리돼 깔끔하다.)
	bool executeFullscreenBake(
		ID3D12Device* device,
		ID3D12PipelineState* pipelineState,
		ID3D12RootSignature* rootSignature,
		JRenderTarget* target)
	{
		if (device == nullptr || pipelineState == nullptr || rootSignature == nullptr || target == nullptr || !target->IsValid())
		{
			return false;
		}

		ComPtr<ID3D12CommandQueue> commandQueue;
		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12GraphicsCommandList> commandList;
		ComPtr<ID3D12Fence> fence;

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)))
			|| FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)))
			|| FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), pipelineState, IID_PPV_ARGS(&commandList)))
			|| FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
		{
			return false;
		}

		const uint32 width = target->GetWidth();
		const uint32 height = target->GetHeight();
		D3D12_CPU_DESCRIPTOR_HANDLE rtv = target->GetColorRTV(0, 0);

		const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		const D3D12_RECT scissor = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

		commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissor);
		commandList->SetGraphicsRootSignature(rootSignature);
		commandList->SetPipelineState(pipelineState);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0); // full-screen triangle

		// 이후 LightingPass가 SRV로 읽도록 상태 전환.
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			target->GetResource(0),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);

		if (FAILED(commandList->Close()))
		{
			return false;
		}

		ID3D12CommandList* lists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, lists);

		HANDLE fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (fenceEvent == nullptr)
		{
			return false;
		}
		commandQueue->Signal(fence.Get(), 1);
		if (fence->GetCompletedValue() < 1)
		{
			fence->SetEventOnCompletion(1, fenceEvent);
			::WaitForSingleObject(fenceEvent, INFINITE);
		}
		::CloseHandle(fenceEvent);

		target->SetResourceState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		return true;
	}
}

JReflectionProbe::JReflectionProbe() = default;

JReflectionProbe::~JReflectionProbe()
{
	if (_brdfPipeline != nullptr)
	{
		delete _brdfPipeline;
		_brdfPipeline = nullptr;
	}
	if (_brdfShader != nullptr)
	{
		delete _brdfShader;
		_brdfShader = nullptr;
	}
	if (_irradiancePipeline != nullptr)
	{
		delete _irradiancePipeline;
		_irradiancePipeline = nullptr;
	}
	if (_irradianceShader != nullptr)
	{
		delete _irradianceShader;
		_irradianceShader = nullptr;
	}
	if (_prefilterPipeline != nullptr)
	{
		delete _prefilterPipeline;
		_prefilterPipeline = nullptr;
	}
	if (_prefilterShader != nullptr)
	{
		delete _prefilterShader;
		_prefilterShader = nullptr;
	}
}

bool JReflectionProbe::EnsureBaked(Render::JRenderContext* renderContext)
{
	if (_baked)
	{
		return true;
	}
	if (renderContext == nullptr)
	{
		return false;
	}

	if (!bakeBRDFLUT(renderContext))
	{
		std::cerr << "JReflectionProbe::EnsureBaked failed: BRDF LUT bake failed." << std::endl;
		return false;
	}

	_baked = true;
	return true;
}

const Render::JTexture* JReflectionProbe::GetBRDFLUT() const
{
	return _brdfLUT != nullptr ? _brdfLUT->GetTextureView() : nullptr;
}

uint32 JReflectionProbe::GetCaptureCubeSize() const
{
	return CAPTURE_CUBE_SIZE;
}

bool JReflectionProbe::EnsureCaptureCube()
{
	if (_captureCube != nullptr)
	{
		return _captureCube->IsValid();
	}

	// 6면 HDR 큐브. JRenderer가 probe 시점에서 면별로 linear HDR 씬을 렌더한다.
	JRenderTarget::Desc desc;
	desc.width = CAPTURE_CUBE_SIZE;
	desc.height = CAPTURE_CUBE_SIZE;
	desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.type = JRenderTarget::Type::Color;
	desc.cube = true;
	desc.shaderResource = true;
	desc.initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	_captureCube = std::make_unique<JRenderTarget>(desc);
	if (!_captureCube->IsValid() || !_captureCube->HasShaderResource())
	{
		std::cerr << "JReflectionProbe::EnsureCaptureCube failed: cube target creation failed." << std::endl;
		_captureCube.reset();
		return false;
	}
	return true;
}

const Render::JTexture* JReflectionProbe::GetIrradiance() const
{
	return _irradianceReady && _irradianceCube != nullptr ? _irradianceCube->GetTextureView() : nullptr;
}

bool JReflectionProbe::ensureIrradianceResources(Render::JRenderContext* renderContext)
{
	if (_irradianceCube == nullptr)
	{
		JRenderTarget::Desc desc;
		desc.width = IRRADIANCE_SIZE;
		desc.height = IRRADIANCE_SIZE;
		desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.type = JRenderTarget::Type::Color;
		desc.cube = true;
		desc.shaderResource = true;
		desc.initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		_irradianceCube = std::make_unique<JRenderTarget>(desc);
		if (!_irradianceCube->IsValid() || !_irradianceCube->HasShaderResource())
		{
			std::cerr << "JReflectionProbe: irradiance cube creation failed." << std::endl;
			_irradianceCube.reset();
			return false;
		}
	}

	if (_irradianceShader == nullptr)
	{
		const std::string shaderPath = get_Engine_Shader_Path() + "\\irradiance_convolve.hlsl";
		_irradianceShader = renderContext->CreateShader(shaderPath);
		if (_irradianceShader == nullptr)
		{
			std::cerr << "JReflectionProbe: irradiance shader creation failed: " << shaderPath << std::endl;
			return false;
		}
	}

	if (_irradiancePipeline == nullptr)
	{
		_irradiancePipeline = renderContext->CreatePipeline(
			_irradianceShader, false, false, false, false,
			{ DXGI_FORMAT_R16G16B16A16_FLOAT }, false,
			D3D12_DEPTH_WRITE_MASK_ALL, D3D12_COMPARISON_FUNC_GREATER, false);
		if (_irradiancePipeline == nullptr)
		{
			std::cerr << "JReflectionProbe: irradiance pipeline creation failed." << std::endl;
			return false;
		}
	}

	return true;
}

bool JReflectionProbe::RecordIrradiance(Render::JCommandQueue* commandQueue, Render::JRenderContext* renderContext)
{
	if (_irradianceReady)
	{
		return true;
	}
	if (commandQueue == nullptr || renderContext == nullptr || _captureCube == nullptr || !_captureCube->IsValid())
	{
		return false;
	}
	if (!ensureIrradianceResources(renderContext))
	{
		return false;
	}

	Render::JTexture* envTexture = _captureCube->GetTextureView();
	if (envTexture == nullptr)
	{
		return false;
	}

	JColor black;
	black.r = 0.0f; black.g = 0.0f; black.b = 0.0f; black.a = 1.0f;
	const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(IRRADIANCE_SIZE), static_cast<float>(IRRADIANCE_SIZE), 0.0f, 1.0f };
	const D3D12_RECT scissor = { 0, 0, static_cast<LONG>(IRRADIANCE_SIZE), static_cast<LONG>(IRRADIANCE_SIZE) };

	for (uint32 face = 0; face < 6; ++face)
	{
		PerFaceConstants faceConstants{};
		faceConstants.forward = FACE_FORWARD[face];
		faceConstants.up = FACE_UP[face];
		faceConstants.right = crossVec(FACE_UP[face], FACE_FORWARD[face]);
		const D3D12_GPU_VIRTUAL_ADDRESS faceGpuAddress = commandQueue->UploadFrameConstantBuffer(&faceConstants, sizeof(faceConstants));

		commandQueue->BeginRenderPassFace(_irradianceCube.get(), face, 0, black, 0);
		commandQueue->SetViewports(1, &viewport);
		commandQueue->SetScissorRects(1, &scissor);

		Render::JGraphicResource graphicResource(_irradianceShader);
		graphicResource.SetConstantBufferAddress("PerFace", faceGpuAddress);
		graphicResource.SetTexture("EnvCube", envTexture);

		commandQueue->SetPipeline(_irradiancePipeline);
		commandQueue->SetGraphicResources(&graphicResource);
		commandQueue->DrawFullscreenTriangle();
		commandQueue->EndRenderPass();
	}

	commandQueue->TransitionRenderTarget(_irradianceCube.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_irradianceReady = true;
	return true;
}

const Render::JTexture* JReflectionProbe::GetPrefiltered() const
{
	return _prefilteredReady && _prefilteredCube != nullptr ? _prefilteredCube->GetTextureView() : nullptr;
}

uint32 JReflectionProbe::GetPrefilteredMipCount() const
{
	return PREFILTER_MIPS;
}

bool JReflectionProbe::ensurePrefilteredResources(Render::JRenderContext* renderContext)
{
	if (_prefilteredCube == nullptr)
	{
		JRenderTarget::Desc desc;
		desc.width = PREFILTER_SIZE;
		desc.height = PREFILTER_SIZE;
		desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		desc.type = JRenderTarget::Type::Color;
		desc.cube = true;
		desc.mipLevels = static_cast<uint16>(PREFILTER_MIPS);
		desc.shaderResource = true;
		desc.initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		_prefilteredCube = std::make_unique<JRenderTarget>(desc);
		if (!_prefilteredCube->IsValid() || !_prefilteredCube->HasShaderResource())
		{
			std::cerr << "JReflectionProbe: prefiltered cube creation failed." << std::endl;
			_prefilteredCube.reset();
			return false;
		}
	}

	if (_prefilterShader == nullptr)
	{
		const std::string shaderPath = get_Engine_Shader_Path() + "\\prefilter_env.hlsl";
		_prefilterShader = renderContext->CreateShader(shaderPath);
		if (_prefilterShader == nullptr)
		{
			std::cerr << "JReflectionProbe: prefilter shader creation failed: " << shaderPath << std::endl;
			return false;
		}
	}

	if (_prefilterPipeline == nullptr)
	{
		_prefilterPipeline = renderContext->CreatePipeline(
			_prefilterShader, false, false, false, false,
			{ DXGI_FORMAT_R16G16B16A16_FLOAT }, false,
			D3D12_DEPTH_WRITE_MASK_ALL, D3D12_COMPARISON_FUNC_GREATER, false);
		if (_prefilterPipeline == nullptr)
		{
			std::cerr << "JReflectionProbe: prefilter pipeline creation failed." << std::endl;
			return false;
		}
	}

	return true;
}

bool JReflectionProbe::RecordPrefiltered(Render::JCommandQueue* commandQueue, Render::JRenderContext* renderContext)
{
	if (_prefilteredReady)
	{
		return true;
	}
	if (commandQueue == nullptr || renderContext == nullptr || _captureCube == nullptr || !_captureCube->IsValid())
	{
		return false;
	}
	if (!ensurePrefilteredResources(renderContext))
	{
		return false;
	}

	Render::JTexture* envTexture = _captureCube->GetTextureView();
	if (envTexture == nullptr)
	{
		return false;
	}

	JColor black;
	black.r = 0.0f; black.g = 0.0f; black.b = 0.0f; black.a = 1.0f;

	for (uint32 mip = 0; mip < PREFILTER_MIPS; ++mip)
	{
		const uint32 mipSize = std::max<uint32>(PREFILTER_SIZE >> mip, 1u);
		const float roughness = PREFILTER_MIPS > 1 ? static_cast<float>(mip) / static_cast<float>(PREFILTER_MIPS - 1) : 0.0f;
		const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(mipSize), static_cast<float>(mipSize), 0.0f, 1.0f };
		const D3D12_RECT scissor = { 0, 0, static_cast<LONG>(mipSize), static_cast<LONG>(mipSize) };

		for (uint32 face = 0; face < 6; ++face)
		{
			PerPrefilterConstants constants{};
			constants.forward = FACE_FORWARD[face];
			constants.up = FACE_UP[face];
			constants.right = crossVec(FACE_UP[face], FACE_FORWARD[face]);
			constants.params = { roughness, 0.0f, 0.0f, 0.0f };
			const D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = commandQueue->UploadFrameConstantBuffer(&constants, sizeof(constants));

			commandQueue->BeginRenderPassFace(_prefilteredCube.get(), face, mip, black, 0);
			commandQueue->SetViewports(1, &viewport);
			commandQueue->SetScissorRects(1, &scissor);

			Render::JGraphicResource graphicResource(_prefilterShader);
			graphicResource.SetConstantBufferAddress("PerFace", gpuAddress);
			graphicResource.SetTexture("EnvCube", envTexture);

			commandQueue->SetPipeline(_prefilterPipeline);
			commandQueue->SetGraphicResources(&graphicResource);
			commandQueue->DrawFullscreenTriangle();
			commandQueue->EndRenderPass();
		}
	}

	commandQueue->TransitionRenderTarget(_prefilteredCube.get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_prefilteredReady = true;
	return true;
}

bool JReflectionProbe::bakeBRDFLUT(Render::JRenderContext* renderContext)
{
	if (GetEngine() == nullptr || GetEngine()->GetDevice() == nullptr)
	{
		return false;
	}
	ComPtr<ID3D12Device> device = GetEngine()->GetDevice()->GetDevice();
	if (device == nullptr)
	{
		return false;
	}

	// RG16F 2D 타겟. (NdotV, roughness) → (scale, bias)
	JRenderTarget::Desc desc;
	desc.width = BRDF_LUT_SIZE;
	desc.height = BRDF_LUT_SIZE;
	desc.format = DXGI_FORMAT_R16G16_FLOAT;
	desc.type = JRenderTarget::Type::Color;
	desc.shaderResource = true;
	desc.initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	_brdfLUT = std::make_unique<JRenderTarget>(desc);
	if (!_brdfLUT->IsValid() || !_brdfLUT->HasShaderResource())
	{
		std::cerr << "JReflectionProbe::bakeBRDFLUT failed: render target creation failed." << std::endl;
		return false;
	}

	const std::string shaderPath = get_Engine_Shader_Path() + "\\brdf_lut.hlsl";
	_brdfShader = renderContext->CreateShader(shaderPath);
	if (_brdfShader == nullptr)
	{
		std::cerr << "JReflectionProbe::bakeBRDFLUT failed: shader creation failed: " << shaderPath << std::endl;
		return false;
	}

	// 풀스크린 삼각형: 정점 입력/깊이 없음, RTV는 RG16F.
	_brdfPipeline = renderContext->CreatePipeline(
		_brdfShader,
		false,                              // alpha blend
		false,                              // vertex input
		false,                              // normal input
		false,                              // texcoord input
		{ DXGI_FORMAT_R16G16_FLOAT },       // rtv formats
		false,                              // depth
		D3D12_DEPTH_WRITE_MASK_ALL,
		D3D12_COMPARISON_FUNC_GREATER,
		false);                             // useDefaultRtvFormat
	if (_brdfPipeline == nullptr || _brdfShader->GetRootSignature() == nullptr)
	{
		std::cerr << "JReflectionProbe::bakeBRDFLUT failed: pipeline creation failed." << std::endl;
		return false;
	}

	return executeFullscreenBake(
		device.Get(),
		_brdfPipeline->pipelineState.Get(),
		_brdfShader->GetRootSignature()->signature.Get(),
		_brdfLUT.get());
}

J_ENGINE_END
