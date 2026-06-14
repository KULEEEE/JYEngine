#include "engine/render/JRenderer.h"

#include "engine/render/JRenderDB.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JCommandQueue.h"
#include "engine/render/JRenderPass.h"
#include "engine/render/JRenderTarget.h"
#include "engine/render/JSceneColorPass.h"
#include "engine/render/JDrawItemCache.h"
#include "engine/render/JDepthPass.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JGBufferPass.h"
#include "engine/render/JLightingPass.h"
#include "engine/render/JSSAOPass.h"
#include "engine/render/JForwardOverlayPass.h"
#include "engine/render/JShadowMap.h"
#include "engine/render/JShadowPass.h"
#include "engine/render/JReflectionProbe.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"

#include <iostream>

J_ENGINE_BEGIN

JRenderer::JRenderer() = default;
JRenderer::~JRenderer() = default;

void JRenderer::initializeDefaultPasses()
{
	if (_renderPath == RenderPath::Deferred)
	{
		initializeDeferredPasses();
		return;
	}

	initializeForwardPasses();
}

void JRenderer::initializeForwardPasses()
{
	_passes.clear();
	_passes.push_back(std::make_unique<JSceneColorPass>());
}

void JRenderer::initializeDeferredPasses()
{
	_passes.clear();
	_passes.push_back(std::make_unique<JShadowPass>());
	_passes.push_back(std::make_unique<JDepthPass>());
	_passes.push_back(std::make_unique<JGBufferPass>());
	_passes.push_back(std::make_unique<JSSAOPass>());
	_passes.push_back(std::make_unique<JLightingPass>());
	_passes.push_back(std::make_unique<JForwardOverlayPass>());
}

void JRenderer::Initialize(Render::JCommandQueue* commandQueue, Render::JRenderContext* renderContext)
{
	_commandQueue = commandQueue;
	_renderContext = renderContext;
	_renderDB.Initialize(renderContext);
	initializeDefaultPasses();
}

void JRenderer::SetRenderPath(RenderPath renderPath)
{
	if (_renderPath == renderPath)
	{
		return;
	}

	_renderPath = renderPath;
	initializeDefaultPasses();
}

void JRenderer::ensureGBuffer(const FrameDesc& frameDesc)
{
	if (_renderPath != RenderPath::Deferred || frameDesc.renderTarget == nullptr)
	{
		return;
	}

	const uint32 width = std::max(frameDesc.renderTarget->GetWidth(), 1u);
	const uint32 height = std::max(frameDesc.renderTarget->GetHeight(), 1u);
	if (_gBuffer != nullptr && _gBuffer->IsValid() && _gBuffer->GetWidth() == width && _gBuffer->GetHeight() == height)
	{
		return;
	}

	JGBuffer::Desc desc;
	desc.width = width;
	desc.height = height;
	_gBuffer = std::make_unique<JGBuffer>();
	_gBuffer->Initialize(desc);
}

void JRenderer::ensureShadowMap()
{
	if (_renderPath != RenderPath::Deferred || _shadowMap != nullptr)
	{
		return;
	}

	_shadowMap = std::make_unique<JShadowMap>();
	if (!_shadowMap->Initialize(JShadowMap::Desc{}))
	{
		std::cerr << "JRenderer::ensureShadowMap failed: shadow map initialization failed." << std::endl;
		_shadowMap.reset();
	}
}

void JRenderer::ensureSSAOTarget(const FrameDesc& frameDesc)
{
	if (_renderPath != RenderPath::Deferred || frameDesc.renderTarget == nullptr)
	{
		return;
	}

	const uint32 width = std::max(frameDesc.renderTarget->GetWidth(), 1u);
	const uint32 height = std::max(frameDesc.renderTarget->GetHeight(), 1u);
	if (_ssaoTarget != nullptr && _ssaoTarget->IsValid() && _ssaoTarget->GetWidth() == width && _ssaoTarget->GetHeight() == height)
	{
		return;
	}

	JRenderTarget::Desc desc;
	desc.width = width;
	desc.height = height;
	desc.format = DXGI_FORMAT_R8_UNORM;
	desc.type = JRenderTarget::Type::Color;
	desc.shaderResource = true;
	desc.initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	desc.clearColor = JColor(1.0f, 1.0f, 1.0f, 1.0f); // 1 = 차폐 없음
	_ssaoTarget = std::make_unique<JRenderTarget>(desc);
	if (!_ssaoTarget->IsValid())
	{
		std::cerr << "JRenderer::ensureSSAOTarget failed: SSAO target creation failed." << std::endl;
		_ssaoTarget.reset();
	}
}

void JRenderer::ensureReflectionProbe()
{
	if (_renderPath != RenderPath::Deferred || _renderContext == nullptr)
	{
		return;
	}

	if (_reflectionProbe == nullptr)
	{
		_reflectionProbe = std::make_unique<JReflectionProbe>();
	}

	// 정적 씬: 최초 1회만 bake되고 이후 프레임은 들고 있는 텍스처를 샘플만 한다.
	_reflectionProbe->EnsureBaked(_renderContext);
}

void JRenderer::Render(const FrameDesc& frameDesc)
{
	if (_commandQueue == nullptr || _renderContext == nullptr)
	{
		std::cerr << "JRenderer::Render skipped: command queue or render context is null." << std::endl;
		return;
	}

	if (frameDesc.renderTarget == nullptr)
	{
		std::cerr << "JRenderer::Render skipped: render target is null." << std::endl;
		return;
	}

	if (_passes.empty())
	{
		initializeDefaultPasses();
	}
	ensureGBuffer(frameDesc);
	ensureShadowMap();
	ensureSSAOTarget(frameDesc);
	ensureReflectionProbe();
	prepareFrameResources(frameDesc);

	const JCameraResource* cameraResource = _renderDB.FindCameraResource(frameDesc.camera);
	if (cameraResource == nullptr)
	{
		std::cerr << "JRenderer::Render skipped: camera resource is not ready." << std::endl;
		return;
	}

	JRenderPassContext context;
	context.commandQueue = _commandQueue;
	context.renderContext = _renderContext;
	context.renderDB = &_renderDB;
	context.gBuffer = _gBuffer.get();
	context.shadowMap = _shadowMap.get();
	context.reflectionProbe = _reflectionProbe.get();
	context.ssaoTarget = _ssaoTarget.get();

	for (const std::unique_ptr<JRenderPass>& pass : _passes)
	{
		if (pass != nullptr)
		{
			pass->Execute(context, frameDesc);
		}
	}

	// 메인 패스(shadow/gbuffer/lighting)가 끝난 뒤, 정적 씬을 1회 큐브에 캡처한다.
	// 같은 command list에 이어 기록되며, 캡처된 큐브는 다음 프레임부터 IBL 소스로 쓰인다.
	captureReflectionProbe(frameDesc);
}

void JRenderer::captureReflectionProbe(const FrameDesc& frameDesc)
{
	if (_renderPath != RenderPath::Deferred || _reflectionProbe == nullptr || _reflectionProbe->IsCaptured())
	{
		return;
	}
	if (_commandQueue == nullptr || _renderContext == nullptr || _shadowMap == nullptr)
	{
		return;
	}
	if (!_reflectionProbe->EnsureCaptureCube())
	{
		return;
	}

	JRenderTarget* cube = _reflectionProbe->GetCaptureCube();
	if (cube == nullptr)
	{
		return;
	}

	const uint32 faceSize = _reflectionProbe->GetCaptureCubeSize();

	// 캡처 전용 GBuffer(face 해상도). 메인 GBuffer와 분리해 thrash를 막는다.
	if (_captureGBuffer == nullptr || !_captureGBuffer->IsValid()
		|| _captureGBuffer->GetWidth() != faceSize || _captureGBuffer->GetHeight() != faceSize)
	{
		JGBuffer::Desc gbDesc;
		gbDesc.width = faceSize;
		gbDesc.height = faceSize;
		_captureGBuffer = std::make_unique<JGBuffer>();
		if (!_captureGBuffer->Initialize(gbDesc))
		{
			std::cerr << "JRenderer::captureReflectionProbe failed: capture gbuffer init failed." << std::endl;
			_captureGBuffer.reset();
			return;
		}
	}

	if (_captureDepthPass == nullptr) { _captureDepthPass = std::make_unique<JDepthPass>(); }
	if (_captureGBufferPass == nullptr) { _captureGBufferPass = std::make_unique<JGBufferPass>(); }
	if (_captureLightingPass == nullptr) { _captureLightingPass = std::make_unique<JLightingPass>(); }

	JRenderPassContext captureContext;
	captureContext.commandQueue = _commandQueue;
	captureContext.renderContext = _renderContext;
	captureContext.renderDB = &_renderDB;
	captureContext.gBuffer = _captureGBuffer.get();
	captureContext.shadowMap = _shadowMap.get();

	// probe 시점에서는 메인 frustum 컬링 결과 대신 모든 opaque draw item을 그린다(전방향 캡처).
	std::vector<uint32> opaqueIndices;
	if (frameDesc.drawItemCache != nullptr)
	{
		const std::vector<JDrawItem>& drawItems = frameDesc.drawItemCache->drawItems;
		opaqueIndices.reserve(drawItems.size());
		for (uint32 i = 0; i < static_cast<uint32>(drawItems.size()); ++i)
		{
			if (!drawItems[i].transparent)
			{
				opaqueIndices.push_back(i);
			}
		}
	}

	// probe 위치 = 주 카메라 위치.
	const XMVECTOR probePosition = XMVectorSet(
		frameDesc.cameraWorldPosition.x, frameDesc.cameraWorldPosition.y, frameDesc.cameraWorldPosition.z, 1.0f);

	// D3D11/12 TextureCube 면 순서/방향 (LH): +X,-X,+Y,-Y,+Z,-Z
	const XMVECTOR faceDir[6] =
	{
		XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),  XMVectorSet(-1.0f, 0.0f, 0.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),  XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f),
		XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),  XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f),
	};
	const XMVECTOR faceUp[6] =
	{
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),  XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
		XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),  XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f),
	};

	// snapshot builder와 동일한 reverse-Z 규약: PerspectiveFovLH(fov, aspect, far, near)로 near/far를 스왑.
	const float nearPlane = 0.1f;
	const float farPlane = 1000.0f;
	const XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, farPlane, nearPlane);

	const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(faceSize), static_cast<float>(faceSize), 0.0f, 1.0f };
	const D3D12_RECT scissor = { 0, 0, static_cast<LONG>(faceSize), static_cast<LONG>(faceSize) };

	for (uint32 face = 0; face < 6; ++face)
	{
		const XMMATRIX view = XMMatrixLookToLH(probePosition, faceDir[face], faceUp[face]);
		const XMMATRIX viewProjection = XMMatrixMultiply(view, projection);

		// gbuffer/depth 패스가 읽는 PerFrame VP를 이 면의 VP로 갱신한다(업로드는 record 시점에 복사됨).
		_renderDB.SyncCamera(frameDesc.camera, viewProjection);

		FrameDesc faceFrame = frameDesc;
		faceFrame.renderTarget = cube;
		faceFrame.renderTargetSlice = face;
		faceFrame.captureMode = true;
		faceFrame.viewport = viewport;
		faceFrame.scissorRect = scissor;
		faceFrame.cameraViewProjection = viewProjection;
		faceFrame.cameraInverseViewProjection = XMMatrixInverse(nullptr, viewProjection);
		faceFrame.opaqueDrawItemIndices = JArrayView<const uint32>(opaqueIndices);

		_captureDepthPass->Execute(captureContext, faceFrame);
		_captureGBufferPass->Execute(captureContext, faceFrame);
		_captureLightingPass->Execute(captureContext, faceFrame);
	}

	// 큐브를 SRV 상태로 전환 → IBL/convolution 소스로 샘플.
	_commandQueue->TransitionRenderTarget(cube, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// 같은 프레임에서 바로 diffuse irradiance + specular prefiltered를 컨볼브해 둔다.
	_reflectionProbe->RecordIrradiance(_commandQueue, _renderContext);
	_reflectionProbe->RecordPrefiltered(_commandQueue, _renderContext);

	// 캡처에 쓴 카메라 VP를 메인 값으로 원복(다음 프레임 prepareFrameResources가 다시 동기화하지만 안전하게).
	_renderDB.SyncCamera(frameDesc.camera, frameDesc.cameraViewProjection);

	_reflectionProbe->MarkCaptured();
}

void JRenderer::prepareFrameResources(const FrameDesc& frameDesc)
{
	for (const JFrameMaterialSnapshot& snapshot : frameDesc.materialSnapshots)
	{
		if (snapshot.source == nullptr)
		{
			continue;
		}

		if (snapshot.source->IsDirty() || _renderDB.FindMaterialResource(snapshot.material) == nullptr)
		{
			_renderDB.SyncMaterial(snapshot.material, *snapshot.source);
			snapshot.source->ClearDirty();
		}
	}

	if (frameDesc.camera.IsValid())
	{
		_renderDB.SyncCamera(frameDesc.camera, frameDesc.cameraViewProjection);
	}

	for (JFrameTransformSnapshot& snapshot : frameDesc.transformSnapshots)
	{
		if (!snapshot.valid || !snapshot.dirty)
		{
			continue;
		}

		_renderDB.SyncTransform(snapshot.transform, snapshot.world);
		snapshot.dirty = false;
	}

	_renderDB.SyncLight(frameDesc.lightItems);

	std::unordered_set<const JMesh*> activeMeshes;
	if (frameDesc.drawItemCache != nullptr)
	{
		for (const JDrawItem& drawItem : frameDesc.drawItemCache->drawItems)
		{
			if (drawItem.mesh != nullptr)
			{
				activeMeshes.insert(drawItem.mesh);
			}
		}
	}

	for (const JMesh* mesh : activeMeshes)
	{
		_renderDB.GetOrCreateMeshResource(mesh);
	}

	if (frameDesc.drawItemCache != nullptr)
	{
		for (JDrawItem& drawItem : frameDesc.drawItemCache->drawItems)
		{
			_renderDB.UpdateDrawItemResourceIndices(drawItem);
		}
	}
}

J_ENGINE_END
