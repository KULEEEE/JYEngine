#pragma once

#ifndef __J_RENDER_CONTEXT_H__
#define __J_RENDER_CONTEXT_H__

#include "engine/precompile.h"
#include "engine/platform/JDevice.h"
#include "engine/render/JRenderDefinition.h"
#include "engine/asset/JShader.h"
#include "engine/asset/JTextureFile.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <fstream>
#include <wincodec.h>

J_RENDER_BEGIN

class JRenderContext
{
public:
	JRenderContext() = delete;
	JRenderContext(JDevice* device);
	~JRenderContext();

	bool BeginUploadBatch();
	bool EndUploadBatch();

	template<typename T>
	JVertexBuffer* CreateVertexBuffer(const std::vector<T>& data, size_t vertexCount)
	{
		if (data.empty() || vertexCount == 0)
		{
			return nullptr;
		}

		size_t size = sizeof(T) * data.size();

		ComPtr<ID3D12Device> device = _device->GetDevice();

		JVertexBuffer* vBuffer = new JVertexBuffer();
		ID3D12Resource*& buffer = vBuffer->buffer;
		D3D12_VERTEX_BUFFER_VIEW& view = vBuffer->view;

		// static draw buffer는 DEFAULT heap에 두고, UPLOAD heap은 staging으로만 사용함.
		if (!createDefaultBuffer(data.data(), size, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &buffer))
		{
			delete vBuffer;
			return nullptr;
		}

		view.BufferLocation = (buffer)->GetGPUVirtualAddress();
		view.StrideInBytes = static_cast<uint32>(size / vertexCount);
		view.SizeInBytes = static_cast<uint32>(size);

		return vBuffer;
	}

	JIndexBuffer* CreateIndexBuffer(const std::vector<uint32>& data)
	{
		if (data.empty())
		{
			return nullptr;
		}

		size_t size = sizeof(uint32) * data.size();
		ComPtr<ID3D12Device> device = _device->GetDevice();

		JIndexBuffer* iBuffer = new JIndexBuffer();
		ID3D12Resource*& buffer = iBuffer->buffer;
		D3D12_INDEX_BUFFER_VIEW& view = iBuffer->view;

		// index buffer도 draw 중 반복 읽기 대상이라 DEFAULT heap으로 생성함.
		if (!createDefaultBuffer(data.data(), size, D3D12_RESOURCE_STATE_INDEX_BUFFER, &buffer))
		{
			delete iBuffer;
			return nullptr;
		}

		view.BufferLocation = (buffer)->GetGPUVirtualAddress();
		view.Format = DXGI_FORMAT_R32_UINT;
		view.SizeInBytes = static_cast<uint32>(size);

		return iBuffer;
	}
	
	void DestroyVertexBuffer(JVertexBuffer* buffer)
	{
		if (buffer == nullptr)
		{
			return;
		}

		buffer->Destroy();
		delete buffer;
	}

	void DestroyIndexBuffer(JIndexBuffer* buffer)
	{
		if (buffer == nullptr)
		{
			return;
		}

		buffer->Destroy();
		delete buffer;
	}

	JConstantBuffer* CreateConstantBuffer(void* buffer, size_t size)
	{
		ComPtr<ID3D12Device> device = _device->GetDevice();

		// D3D12 constant buffer view는 256 byte 정렬이 필요함.
		size_t bufferSize = (size + 255) & ~255;

		JConstantBuffer* cBuffer = new JConstantBuffer();

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
		
		HRESULT hr = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&cBuffer->buffer)
		);
		
		void* pData;
		D3D12_RANGE range = {};
		cBuffer->buffer->Map(0, &range, &pData);

		::memset(pData, 0, bufferSize);
		::memcpy(pData, buffer, size);

		cBuffer->buffer->Unmap(0, nullptr);

		return cBuffer;
	}

	void DestroyConstantBuffer(JConstantBuffer* buffer)
	{
		if (buffer == nullptr)
		{
			return;
		}

		buffer->Destroy();
		delete buffer;
	}

	JTexture* CreateSolidColorTexture(const JColor& color)
	{
		ComPtr<ID3D12Device> device = _device->GetDevice();
		if (device == nullptr)
		{
			return nullptr;
		}

		JTexture* texture = new JTexture();

		// fallback용 1x1 texture. 실제 샘플링은 DEFAULT heap에서 하도록 만듦.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = 1;
		textureDesc.Height = 1;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
		HRESULT hr = device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&texture->texture));
		if (FAILED(hr) || texture->texture == nullptr)
		{
			delete texture;
			return nullptr;
		}

		const uint8 pixelData[4] =
		{
			static_cast<uint8>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(std::clamp(color.a, 0.0f, 1.0f) * 255.0f)
		};

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		UINT rowCount = 0;
		UINT64 rowSizeInBytes = 0;
		UINT64 uploadBufferSize = 0;
		// texture copy는 row pitch 정렬이 필요하므로 footprint를 device에 물어봄.
		device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &rowCount, &rowSizeInBytes, &uploadBufferSize);

		ComPtr<ID3D12Resource> uploadBuffer;
		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		hr = device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer));
		if (FAILED(hr) || uploadBuffer == nullptr)
		{
			delete texture;
			return nullptr;
		}

		void* mappedData = nullptr;
		D3D12_RANGE readRange = {};
		hr = uploadBuffer->Map(0, &readRange, &mappedData);
		if (FAILED(hr) || mappedData == nullptr)
		{
			delete texture;
			return nullptr;
		}

		uint8* uploadBytes = static_cast<uint8*>(mappedData) + footprint.Offset;
		memcpy(uploadBytes, pixelData, sizeof(pixelData));
		uploadBuffer->Unmap(0, nullptr);

		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12GraphicsCommandList> commandList;
		ComPtr<ID3D12CommandQueue> commandQueue;
		ComPtr<ID3D12Fence> fence;
		HANDLE fenceEvent = nullptr;

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		CD3DX12_TEXTURE_COPY_LOCATION dst(texture->texture, 0);
		CD3DX12_TEXTURE_COPY_LOCATION src(uploadBuffer.Get(), footprint);
		commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			texture->texture,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		hr = commandList->Close();
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		ID3D12CommandList* lists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, lists);

		hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (fenceEvent == nullptr)
		{
			delete texture;
			return nullptr;
		}

		commandQueue->Signal(fence.Get(), 1);
		if (fence->GetCompletedValue() < 1)
		{
			fence->SetEventOnCompletion(1, fenceEvent);
			::WaitForSingleObject(fenceEvent, INFINITE);
		}
		::CloseHandle(fenceEvent);

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&texture->srvHeap));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(texture->texture, &srvDesc, texture->srvHeap->GetCPUDescriptorHandleForHeapStart());

		return texture;
	}

	void DestroyTexture(JTexture* texture)
	{
		if (texture == nullptr)
		{
			return;
		}

		texture->Destroy();
		delete texture;
	}
	JTexture* CreateTextureFromFile(const std::string& path)
	{
		ComPtr<ID3D12Device> device = _device->GetDevice();
		if (device == nullptr)
		{
			return nullptr;
		}

		// 임포터가 구운 .jtex는 전용 리더로 읽는다. cooked 포맷이 깨진 건 데이터 오류이므로 WIC 폴백 없음.
		std::string extension = std::filesystem::path(path).extension().string();
		for (char& ch : extension)
		{
			ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
		}
		if (extension == ".jtex")
		{
			JTexture* texture = createTextureFromJTex(path);
			if (texture == nullptr)
			{
				std::cerr << "CreateTextureFromFile failed: invalid jtex. path=" << path << std::endl;
			}
			return texture;
		}

		HRESULT coHr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		
		if (FAILED(coHr) && coHr != RPC_E_CHANGED_MODE)
		{
			std::cerr << "CreateTextureFromFile failed: COM initialization failed. path=" << path << std::endl;
			return nullptr;
		}

		// WIC로 파일을 RGBA8 픽셀 배열로 디코딩함.
		ComPtr<IWICImagingFactory> factory;
		HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
		if (FAILED(hr) || factory == nullptr)
		{			std::cerr << "CreateTextureFromFile failed: WIC factory creation failed. path=" << path << std::endl;
			return nullptr;
		}

		std::wstring widePath(path.begin(), path.end());
		ComPtr<IWICBitmapDecoder> decoder;
		hr = factory->CreateDecoderFromFilename(widePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
		if (FAILED(hr) || decoder == nullptr)
		{			std::cerr << "CreateTextureFromFile failed: image decode failed. path=" << path << std::endl;
			return nullptr;
		}

		ComPtr<IWICBitmapFrameDecode> frame;
		hr = decoder->GetFrame(0, &frame);
		if (FAILED(hr) || frame == nullptr)
		{			return nullptr;
		}

		UINT width = 0;
		UINT height = 0;
		frame->GetSize(&width, &height);
		if (width == 0 || height == 0)
		{			return nullptr;
		}

		ComPtr<IWICFormatConverter> converter;
		hr = factory->CreateFormatConverter(&converter);
		if (FAILED(hr) || converter == nullptr)
		{			return nullptr;
		}

		hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
		if (FAILED(hr))
		{			std::cerr << "CreateTextureFromFile failed: pixel format conversion failed. path=" << path << std::endl;
			return nullptr;
		}

		const UINT srcRowPitch = width * 4;
		std::vector<uint8> pixels(static_cast<size_t>(srcRowPitch) * height);
		hr = converter->CopyPixels(nullptr, srcRowPitch, static_cast<UINT>(pixels.size()), pixels.data());
		if (FAILED(hr))
		{			return nullptr;
		}

		return createTexture2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, { { pixels.data(), srcRowPitch, height } });
	}
	JShader* CreateShader(const std::string& path)
	{
#ifdef _DEBUG
		std::cout << "Loading shader: " << path << std::endl;
#endif
		JShader* shader = new JShader(path);
		shader->CompileShader();
		if (!shader->IsReady())
		{
			std::cerr << "CreateShader failed: " << path << std::endl;
			delete shader;
			return nullptr;
		}
		return shader;
	}
	void DestroyShader(JShader* shader) { delete shader; }

	JPipeline* CreatePipeline(
		JShader* shader,
		bool enableAlphaBlend = false,
		bool useVertexInput = true,
		bool useNormalInput = false,
		bool useTexcoordInput = false,
		const std::vector<DXGI_FORMAT>& rtvFormats = {},
		bool enableDepth = true,
		D3D12_DEPTH_WRITE_MASK depthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
		D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_GREATER,
		bool useDefaultRtvFormat = true)
	{
		if (shader == nullptr || shader->GetRootSignature() == nullptr)
		{
			std::cerr << "CreatePipeline failed: shader or root signature is null." << std::endl;
			return nullptr;
		}

		ComPtr<ID3D12Device> device = _device->GetDevice();

		JPipeline* pipeline = new JPipeline();

		D3D12_GRAPHICS_PIPELINE_STATE_DESC& pipelineDesc = pipeline->pipelineDesc;

		// mesh buffer layout에 맞춰 input layout을 선택함.
		D3D12_INPUT_ELEMENT_DESC positionDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_INPUT_ELEMENT_DESC positionNormalDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_INPUT_ELEMENT_DESC positionNormalTexcoordDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		if (!useVertexInput)
		{
			pipelineDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ nullptr, 0 };
		}
		else if (useNormalInput && useTexcoordInput)
		{
			pipelineDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ positionNormalTexcoordDesc, _countof(positionNormalTexcoordDesc) };
		}
		else if (useNormalInput)
		{
			pipelineDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ positionNormalDesc, _countof(positionNormalDesc) };
		}
		else
		{
			pipelineDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ positionDesc, _countof(positionDesc) };
		}

		pipelineDesc.pRootSignature = shader->GetRootSignature()->signature.Get();
		pipelineDesc.VS = shader->GetByteCode()[0];
		pipelineDesc.PS = shader->GetByteCode()[1];
		pipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		if (enableAlphaBlend)
		{
			D3D12_RENDER_TARGET_BLEND_DESC& blendDesc = pipelineDesc.BlendState.RenderTarget[0];
			blendDesc.BlendEnable = TRUE;
			blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
			blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
			blendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		}
		pipelineDesc.SampleMask = UINT_MAX;
		pipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pipelineDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		if (!enableDepth)
		{
			pipelineDesc.DepthStencilState.DepthEnable = FALSE;
			pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		}
		else
		{
			pipelineDesc.DepthStencilState.DepthWriteMask = depthWriteMask;
			pipelineDesc.DepthStencilState.DepthFunc = depthFunc;
		}
		pipelineDesc.DSVFormat = enableDepth ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		const std::vector<DXGI_FORMAT> defaultRtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM };
		const std::vector<DXGI_FORMAT>& activeRtvFormats = (rtvFormats.empty() && useDefaultRtvFormat) ? defaultRtvFormats : rtvFormats;
		pipelineDesc.NumRenderTargets = static_cast<UINT>(activeRtvFormats.size());
		for (UINT rtvIndex = 0; rtvIndex < pipelineDesc.NumRenderTargets && rtvIndex < _countof(pipelineDesc.RTVFormats); ++rtvIndex)
		{
			pipelineDesc.RTVFormats[rtvIndex] = activeRtvFormats[rtvIndex];
			pipelineDesc.BlendState.RenderTarget[rtvIndex].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		}
		pipelineDesc.SampleDesc.Count = 1;

		HRESULT hr = device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipeline->pipelineState));
		if (FAILED(hr)) {
			std::cerr << "CreateGraphicsPipelineState failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
			delete pipeline;
			return nullptr;
		}

		return pipeline;
	}
	void DestroyPipeline(JPipeline* pipeline) { delete pipeline; }

	JRootSignature* CreateRootSignature(JShader* shader)
	{
		JRootSignature* rootSignature = new JRootSignature();
		
		JShader::BindingInfo& bindingInfo = shader->bindingInfo;

		std::vector<CD3DX12_ROOT_PARAMETER> rootParameters;
		std::vector<CD3DX12_DESCRIPTOR_RANGE> descriptorRanges;
		descriptorRanges.reserve(2);

		// shader reflection에서 찾은 cbuffer를 root CBV로 직접 바인딩함.
		for (const auto& cb : bindingInfo.cBuffers) {
			CD3DX12_ROOT_PARAMETER rootParam;
			rootParam.InitAsConstantBufferView(cb.slot);
			rootParameters.push_back(rootParam);
		}

		// texture는 descriptor table 하나로 묶어서 바인딩함.
		if (!bindingInfo.textures.empty()) {
			uint32 srvDescriptorCount = 0;
			for (const auto& texture : bindingInfo.textures)
			{
				srvDescriptorCount = std::max(srvDescriptorCount, texture.slot + 1);
			}
			descriptorRanges.emplace_back();
			descriptorRanges.back().Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, srvDescriptorCount, 0);

			CD3DX12_ROOT_PARAMETER srvRootParam;
			srvRootParam.InitAsDescriptorTable(1, &descriptorRanges.back());
			rootParameters.push_back(srvRootParam);
		}

		// sampler는 root signature에 static sampler로 박아 둠.
		std::vector<CD3DX12_STATIC_SAMPLER_DESC> staticSamplers;
		for (const auto& sampler : bindingInfo.samplers)
		{
			CD3DX12_STATIC_SAMPLER_DESC samplerDesc(sampler.slot);
			if (sampler.name.find("Shadow") != std::string::npos)
			{
				// 이름에 Shadow가 들어가면 SampleCmp용 비교 샘플러로 만듦.
				// reverse-Z shadow map이라 GREATER_EQUAL, 맵 밖은 border(0)와 비교 → ref >= 0 → 항상 lit.
				samplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
				samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
				samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
				samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
				samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
				samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			}
			else
			{
				samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			}
			staticSamplers.push_back(samplerDesc);
		}
		// root parameter와 static sampler를 합쳐 root signature 생성함.
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(static_cast<UINT>(rootParameters.size()), rootParameters.data(), static_cast<UINT>(staticSamplers.size()), staticSamplers.empty() ? nullptr : staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
		if (FAILED(hr)) {
			if (error) {
				OutputDebugStringA((char*)error->GetBufferPointer());
			}
			return nullptr;
		}

		hr = _device->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature->signature));
		if (FAILED(hr)) {
			return nullptr;
		}


		return rootSignature;
	}
	void DestroyRootSignature(JRootSignature* rootSignature) { delete rootSignature; }

private:
	struct TextureSubresource
	{
		const uint8* data = nullptr;
		uint32 rowBytes = 0;   // 한 줄(BC는 블록 행)의 유효 바이트 수
		uint32 rowCount = 0;   // 줄(BC는 블록 행) 수
	};

	// 밉체인을 포함한 2D 텍스처를 생성하고 업로드한다. subresources[m] = m번째 밉.
	JTexture* createTexture2D(DXGI_FORMAT format, uint32 width, uint32 height, const std::vector<TextureSubresource>& subresources)
	{
		ComPtr<ID3D12Device> device = _device->GetDevice();
		if (device == nullptr || width == 0 || height == 0 || subresources.empty())
		{
			return nullptr;
		}

		const uint32 mipCount = static_cast<uint32>(subresources.size());

		JTexture* texture = new JTexture();
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = static_cast<UINT16>(mipCount);
		textureDesc.Format = format;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
		HRESULT hr = device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture->texture));
		if (FAILED(hr) || texture->texture == nullptr)
		{
			delete texture;
			return nullptr;
		}

		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(mipCount);
		std::vector<UINT> rowCounts(mipCount);
		std::vector<UINT64> rowSizes(mipCount);
		UINT64 uploadBufferSize = 0;
		device->GetCopyableFootprints(&textureDesc, 0, mipCount, 0, footprints.data(), rowCounts.data(), rowSizes.data(), &uploadBufferSize);

		ComPtr<ID3D12Resource> uploadBuffer;
		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		hr = device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));
		if (FAILED(hr) || uploadBuffer == nullptr)
		{
			delete texture;
			return nullptr;
		}

		void* mappedData = nullptr;
		D3D12_RANGE readRange = {};
		hr = uploadBuffer->Map(0, &readRange, &mappedData);
		if (FAILED(hr) || mappedData == nullptr)
		{
			delete texture;
			return nullptr;
		}

		for (uint32 mip = 0; mip < mipCount; ++mip)
		{
			const TextureSubresource& subresource = subresources[mip];
			uint8* destination = static_cast<uint8*>(mappedData) + footprints[mip].Offset;
			const uint32 copyRowCount = std::min(rowCounts[mip], subresource.rowCount);
			const size_t copyRowBytes = static_cast<size_t>(std::min<UINT64>(rowSizes[mip], subresource.rowBytes));
			for (uint32 row = 0; row < copyRowCount; ++row)
			{
				// GPU copy footprint의 RowPitch에 맞춰 한 줄씩 복사해야함.
				memcpy(
					destination + static_cast<size_t>(row) * footprints[mip].Footprint.RowPitch,
					subresource.data + static_cast<size_t>(row) * subresource.rowBytes,
					copyRowBytes);
			}
		}
		uploadBuffer->Unmap(0, nullptr);

		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12GraphicsCommandList> commandList;
		ComPtr<ID3D12CommandQueue> commandQueue;
		ComPtr<ID3D12Fence> fence;
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		for (uint32 mip = 0; mip < mipCount; ++mip)
		{
			CD3DX12_TEXTURE_COPY_LOCATION dst(texture->texture, mip);
			CD3DX12_TEXTURE_COPY_LOCATION src(uploadBuffer.Get(), footprints[mip]);
			commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		}
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		hr = commandList->Close();
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		ID3D12CommandList* lists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, lists);
		hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		HANDLE fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (fenceEvent == nullptr)
		{
			delete texture;
			return nullptr;
		}

		commandQueue->Signal(fence.Get(), 1);
		if (fence->GetCompletedValue() < 1)
		{
			fence->SetEventOnCompletion(1, fenceEvent);
			::WaitForSingleObject(fenceEvent, INFINITE);
		}
		::CloseHandle(fenceEvent);

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&texture->srvHeap));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = mipCount;
		device->CreateShaderResourceView(texture->texture, &srvDesc, texture->srvHeap->GetCPUDescriptorHandleForHeapStart());

		return texture;
	}

	// 임포터가 구운 .jtex 컨테이너를 읽어 압축 그대로(밉체인 포함) GPU에 올린다.
	// 소스 포맷(DDS 등) 파싱/검증은 임포트 타임에 끝났으므로 여기서는 무결성 검사만 한다.
	JTexture* createTextureFromJTex(const std::string& path)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file)
		{
			return nullptr;
		}

		const std::streamsize fileSize = file.tellg();
		if (fileSize < static_cast<std::streamsize>(sizeof(JTextureFileHeader)))
		{
			return nullptr;
		}

		std::vector<uint8> bytes(static_cast<size_t>(fileSize));
		file.seekg(0);
		file.read(reinterpret_cast<char*>(bytes.data()), fileSize);
		if (!file)
		{
			return nullptr;
		}

		const JTextureFileHeader* header = reinterpret_cast<const JTextureFileHeader*>(bytes.data());
		if (memcmp(header->magic, JTEXTURE_FILE_MAGIC, sizeof(JTEXTURE_FILE_MAGIC)) != 0
			|| header->version != JTEXTURE_FILE_VERSION)
		{
			return nullptr;
		}

		uint32 bytesPerElement = 0;
		bool blockCompressed = false;
		if (!GetTextureFileFormatLayout(header->dxgiFormat, bytesPerElement, blockCompressed)
			|| header->width == 0
			|| header->height == 0
			|| header->mipCount == 0)
		{
			return nullptr;
		}

		std::vector<TextureSubresource> subresources;
		subresources.reserve(header->mipCount);
		size_t offset = sizeof(JTextureFileHeader);
		for (uint32 mip = 0; mip < header->mipCount; ++mip)
		{
			if (offset + sizeof(JTextureFileMipHeader) > static_cast<size_t>(fileSize))
			{
				return nullptr;
			}

			const JTextureFileMipHeader* mipHeader = reinterpret_cast<const JTextureFileMipHeader*>(bytes.data() + offset);
			offset += sizeof(JTextureFileMipHeader);
			const size_t mipSize = static_cast<size_t>(mipHeader->rowBytes) * mipHeader->rowCount;
			if (mipSize == 0 || offset + mipSize > static_cast<size_t>(fileSize))
			{
				return nullptr;
			}

			subresources.push_back({ bytes.data() + offset, mipHeader->rowBytes, mipHeader->rowCount });
			offset += mipSize;
		}

		return createTexture2D(static_cast<DXGI_FORMAT>(header->dxgiFormat), header->width, header->height, subresources);
	}

	bool createDefaultBuffer(const void* sourceData, size_t size, D3D12_RESOURCE_STATES finalState, ID3D12Resource** outBuffer)
	{
		if (sourceData == nullptr || size == 0 || outBuffer == nullptr)
		{
			return false;
		}

		*outBuffer = nullptr;

		ComPtr<ID3D12Device> device = _device->GetDevice();
		if (device == nullptr)
		{
			return false;
		}

		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
		CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
		// buffer 생성 시 COPY_DEST 초기 상태는 무시될 수 있어서 COMMON으로 만들고 barrier로 전환함.
		HRESULT hr = device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(outBuffer));
		if (FAILED(hr) || *outBuffer == nullptr)
		{
			return false;
		}

		ComPtr<ID3D12Resource> uploadBuffer;
		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		// CPU가 쓸 수 있는 staging buffer에 원본 데이터를 먼저 복사함.
		hr = device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer));
		if (FAILED(hr) || uploadBuffer == nullptr)
		{
			(*outBuffer)->Release();
			*outBuffer = nullptr;
			return false;
		}

		void* mappedData = nullptr;
		D3D12_RANGE readRange = {};
		hr = uploadBuffer->Map(0, &readRange, &mappedData);
		if (FAILED(hr) || mappedData == nullptr)
		{
			(*outBuffer)->Release();
			*outBuffer = nullptr;
			return false;
		}
		memcpy(mappedData, sourceData, size);
		uploadBuffer->Unmap(0, nullptr);

		if (!recordBufferUpload(*outBuffer, uploadBuffer, size, finalState))
		{
			(*outBuffer)->Release();
			*outBuffer = nullptr;
			return false;
		}

		return true;
	}

	bool ensureImmediateUploadContext();
	bool beginUploadCommandList();
	bool recordBufferUpload(ID3D12Resource* destination, ComPtr<ID3D12Resource> uploadBuffer, size_t size, D3D12_RESOURCE_STATES finalState);
	bool executeUploadCommandList();
	void destroyImmediateUploadContext();

	JDevice* _device;

	// DEFAULT heap 업로드용 즉시 command context.
	// batch 중에는 staging buffer를 vector에 잡아 둬서 GPU copy가 끝날 때까지 생존시킴.
	ComPtr<ID3D12CommandQueue> _uploadCommandQueue;
	ComPtr<ID3D12CommandAllocator> _uploadCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> _uploadCommandList;
	ComPtr<ID3D12Fence> _uploadFence;
	uint64 _uploadFenceValue = 1;
	HANDLE _uploadFenceEvent = INVALID_HANDLE_VALUE;
	bool _uploadBatchActive = false;
	bool _uploadCommandListOpen = false;
	std::vector<ComPtr<ID3D12Resource>> _batchedUploadBuffers;
};

J_RENDER_END

#endif








