#include "engine/render/JRenderer.h"

#include "engine/render/JRenderDB.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JRenderPass.h"
#include "engine/render/JRenderTarget.h"
#include "engine/render/JSceneColorPass.h"
#include "engine/render/JDrawItemCache.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JGBufferPass.h"
#include "engine/render/JLightingPass.h"
#include "engine/render/JForwardOverlayPass.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"

#include <iostream>

J_ENGINE_BEGIN

JRenderer::~JRenderer() = default;

uint32 JRenderer::findMaterialIndex(uint32 materialID) const
{
	const auto iter = _materialIndexMap.find(materialID);
	return iter == _materialIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
}

JMaterial* JRenderer::findMaterial(uint32 materialID) const
{
	const uint32 index = findMaterialIndex(materialID);
	return index == static_cast<uint32>(-1) ? nullptr : _materials[index].source;
}

void JRenderer::RegisterMaterial(JMaterial* material)
{
	if (material == nullptr)
	{
		return;
	}

	if (findMaterialIndex(material->instanceID) != static_cast<uint32>(-1))
	{
		MarkMaterialDirty(material);
		return;
	}

	const uint32 newIndex = static_cast<uint32>(_materials.size());
	_materials.push_back({ material->instanceID, material });
	_materialIndexMap[material->instanceID] = newIndex;
	MarkMaterialDirty(material);
}

void JRenderer::UnregisterMaterial(uint32 materialID)
{
	const uint32 index = findMaterialIndex(materialID);
	if (index != static_cast<uint32>(-1))
	{
		const uint32 lastIndex = static_cast<uint32>(_materials.size() - 1);
		if (index != lastIndex)
		{
			_materials[index] = _materials[lastIndex];
			_materialIndexMap[_materials[index].materialID] = index;
		}
		_materials.pop_back();
		_materialIndexMap.erase(materialID);
	}

	for (auto iter = _dirtyMaterialIDs.begin(); iter != _dirtyMaterialIDs.end();)
	{
		if (*iter == materialID)
		{
			iter = _dirtyMaterialIDs.erase(iter);
			continue;
		}
		++iter;
	}

	_renderDB.RemoveMaterialResource(materialID);
}

void JRenderer::MarkMaterialDirty(JMaterial* material)
{
	if (material == nullptr)
	{
		return;
	}

	material->MarkDirty();
	for (uint32 dirtyMaterialID : _dirtyMaterialIDs)
	{
		if (dirtyMaterialID == material->instanceID)
		{
			return;
		}
	}
	_dirtyMaterialIDs.push_back(material->instanceID);
}

void JRenderer::syncMaterials()
{
	for (uint32 materialID : _dirtyMaterialIDs)
	{
		JMaterial* material = findMaterial(materialID);
		if (material == nullptr)
		{
			continue;
		}

		_renderDB.SyncMaterial(*material);
		material->ClearDirty();
	}

	_dirtyMaterialIDs.clear();
}

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
	syncMaterials();

	if (frameDesc.camera.IsValid())
	{
		_renderDB.SyncCamera(frameDesc.camera, frameDesc.cameraViewProjection);
	}

	for (const JFrameTransformSnapshot& snapshot : frameDesc.transformSnapshots)
	{
		_renderDB.SyncTransform(snapshot.transform, snapshot.world);
	}

	JLightSnapshot lightSnapshot;
	for (const JFrameLightSnapshot::Item& item : frameDesc.lightSnapshot.items)
	{
		lightSnapshot.items.push_back({ item.colorIntensity, item.position });
	}
	_renderDB.SyncLight(lightSnapshot);

	std::unordered_set<const JMesh*> activeMeshes;
	if (frameDesc.drawItemCache != nullptr)
	{
		for (uint32 drawItemIndex : frameDesc.opaqueDrawItemIndices)
		{
			if (drawItemIndex < frameDesc.drawItemCache->drawItems.size())
			{
				activeMeshes.insert(frameDesc.drawItemCache->drawItems[drawItemIndex].mesh);
			}
		}
		for (uint32 drawItemIndex : frameDesc.transparentDrawItemIndices)
		{
			if (drawItemIndex < frameDesc.drawItemCache->drawItems.size())
			{
				activeMeshes.insert(frameDesc.drawItemCache->drawItems[drawItemIndex].mesh);
			}
		}
	}

	for (const JMesh* mesh : activeMeshes)
	{
		_renderDB.GetOrCreateMeshResource(mesh);
	}
}

J_ENGINE_END
