#include "engine/render/JRenderer.h"

#include "engine/render/JRenderDB.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JRenderPass.h"
#include "engine/render/JRenderTarget.h"
#include "engine/render/JSceneColorPass.h"
#include "engine/render/JDrawItemCache.h"
#include "engine/render/JDepthPass.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JGBufferPass.h"
#include "engine/render/JLightingPass.h"
#include "engine/render/JForwardOverlayPass.h"
#include "engine/render/JShadowMap.h"
#include "engine/render/JShadowPass.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"

#include <iostream>

J_ENGINE_BEGIN

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

	for (const std::unique_ptr<JRenderPass>& pass : _passes)
	{
		if (pass != nullptr)
		{
			pass->Execute(context, frameDesc);
		}
	}
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
