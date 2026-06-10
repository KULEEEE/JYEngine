#include "engine/render/JRenderServer.h"

#include "engine/asset/JMesh.h"
#include "engine/core/JJobSystem.h"
#include "engine/render/JCameraRenderQueueBuilder.h"
#include "engine/render/JRenderTarget.h"
#include "engine/render/JRenderSnapshotBuilder.h"

J_ENGINE_BEGIN

namespace
{
	JMaterialHandle resolveDrawMaterial(const JScene::RenderObjectComponentData& data, const JMesh::SubMeshInfo& subMesh)
	{
		if (subMesh.materialIndex < data.subMeshMaterials.size())
		{
			const JMaterialHandle material = data.subMeshMaterials[subMesh.materialIndex];
			if (material.IsValid())
			{
				return material;
			}
		}

		return data.material;
	}
}

JRenderServer::~JRenderServer()
{
}

const JCameraSnapshot* JRenderServer::findCameraSnapshot(JCameraHandle camera) const
{
	for (const JCameraSnapshot& snapshot : _frameSnapshot.cameras)
	{
		if (snapshot.camera.index == camera.index && snapshot.camera.generation == camera.generation)
		{
			return &snapshot;
		}
	}

	return nullptr;
}

std::vector<JCameraHandle> JRenderServer::collectSceneCameraHandles(const JScene& scene) const
{
	std::vector<JCameraHandle> cameras;
	const std::vector<JScene::CameraSlot>& slots = scene.GetCameraSlots();
	cameras.reserve(slots.size());
	for (uint32 cameraIndex = 0; cameraIndex < slots.size(); ++cameraIndex)
	{
		const JScene::CameraSlot& slot = slots[cameraIndex];
		if (!slot.active || !slot.data.active)
		{
			continue;
		}

		cameras.push_back({ cameraIndex, slot.generation });
	}

	return cameras;
}

void JRenderServer::updateDrawItemCache(JScene& scene, const std::vector<JSceneRenderObjectEvent>& events, JRenderSnapshotBuilder::Result& outResult)
{
	bool rebuildRequired = !_drawItemCache.initialized;
	for (const JSceneRenderObjectEvent& event : events)
	{
		if (event.type == JSceneRenderObjectEventType::Removed)
		{
			rebuildRequired = true;
			break;
		}
	}

	if (rebuildRequired)
	{
		rebuildDrawItemCache(scene, outResult);
		return;
	}

	for (const JSceneRenderObjectEvent& event : events)
	{
		if (event.type == JSceneRenderObjectEventType::Added)
		{
			appendDrawItems(scene, event.handle, outResult);
		}
		else if (event.type == JSceneRenderObjectEventType::Modified)
		{
			patchDrawItems(scene, event.handle, outResult);
		}
	}

	for (uint32 entityIndex : _drawItemCache.activeDrawEntityIndices)
	{
		if (entityIndex >= _drawItemCache.drawRangeByEntityIndex.size())
		{
			continue;
		}

		const JDrawRange& range = _drawItemCache.drawRangeByEntityIndex[entityIndex];
		if (!range.valid)
		{
			continue;
		}

		for (uint32 i = 0; i < range.count; ++i)
		{
			const uint32 drawItemIndex = range.start + i;
			if (drawItemIndex >= _drawItemCache.drawItems.size())
			{
				continue;
			}

			const JDrawItem& drawItem = _drawItemCache.drawItems[drawItemIndex];
			if (drawItem.mesh != nullptr)
			{
				outResult.activeMeshes.insert(drawItem.mesh);
			}
		}
	}
}

void JRenderServer::rebuildDrawItemCache(const JScene& scene, JRenderSnapshotBuilder::Result& outResult)
{
	_drawItemCache.drawItems.clear();
	_drawItemCache.drawRangeByEntityIndex.clear();
	_drawItemCache.activeDrawEntityIndices.clear();
	_drawItemCache.initialized = true;

	const std::vector<JScene::RenderObjectComponentSlot>& slots = scene.GetRenderObjectComponentSlots();
	for (uint32 renderObjectIndex : scene.GetActiveRenderObjectComponentIndices())
	{
		if (renderObjectIndex >= slots.size())
		{
			continue;
		}

		const JScene::RenderObjectComponentSlot& slot = slots[renderObjectIndex];
		if (slot.active)
		{
			appendDrawItems(scene, { renderObjectIndex, slot.generation }, outResult);
		}
	}
}

void JRenderServer::appendDrawItems(const JScene& scene, JRenderObjectComponentHandle renderObject, JRenderSnapshotBuilder::Result& outResult)
{
	const JScene::RenderObjectComponentData* data = scene.GetRenderObjectComponent(renderObject);
	if (data == nullptr || !data->active || !data->visible || data->mesh == nullptr)
	{
		return;
	}

	const JTransformHandle transform = scene.GetTransformHandle(data->entity);
	if (!transform.IsValid())
	{
		return;
	}

	if (data->entity.index >= _drawItemCache.drawRangeByEntityIndex.size())
	{
		_drawItemCache.drawRangeByEntityIndex.resize(data->entity.index + 1);
	}

	JDrawRange& existingRange = _drawItemCache.drawRangeByEntityIndex[data->entity.index];
	if (existingRange.valid && existingRange.generation == data->entity.generation)
	{
		patchDrawItems(scene, renderObject, outResult);
		return;
	}

	const uint32 start = static_cast<uint32>(_drawItemCache.drawItems.size());
	const std::vector<JMesh::SubMeshInfo>& subMeshes = data->mesh->GetSubMeshInfos();
	if (subMeshes.empty())
	{
		JDrawItem drawItem;
		drawItem.entity = data->entity;
		drawItem.renderObject = renderObject;
		drawItem.transform = transform;
		drawItem.material = data->material;
		drawItem.mesh = data->mesh;
		drawItem.transparent = data->transparent;
		_drawItemCache.drawItems.push_back(drawItem);
	}
	else
	{
		for (uint32 subMeshIndex = 0; subMeshIndex < subMeshes.size(); ++subMeshIndex)
		{
			const JMesh::SubMeshInfo& subMesh = subMeshes[subMeshIndex];
			if (subMesh.endIndex <= subMesh.startIndex)
			{
				continue;
			}

			JDrawItem drawItem;
			drawItem.entity = data->entity;
			drawItem.renderObject = renderObject;
			drawItem.transform = transform;
			drawItem.material = resolveDrawMaterial(*data, subMesh);
			drawItem.mesh = data->mesh;
			drawItem.subMeshIndex = subMeshIndex;
			drawItem.indexCount = subMesh.endIndex - subMesh.startIndex;
			drawItem.startIndex = subMesh.startIndex;
			drawItem.transparent = data->transparent;
			_drawItemCache.drawItems.push_back(drawItem);
		}
	}

	const uint32 count = static_cast<uint32>(_drawItemCache.drawItems.size()) - start;
	if (count == 0)
	{
		return;
	}

	JDrawRange& range = _drawItemCache.drawRangeByEntityIndex[data->entity.index];
	const bool wasTracked = range.tracked;
	range = { start, count, data->entity.generation, true, wasTracked };
	if (!range.tracked)
	{
		_drawItemCache.activeDrawEntityIndices.push_back(data->entity.index);
		range.tracked = true;
	}
	outResult.activeMeshes.insert(data->mesh);
}

void JRenderServer::patchDrawItems(const JScene& scene, JRenderObjectComponentHandle renderObject, JRenderSnapshotBuilder::Result& outResult)
{
	const JScene::RenderObjectComponentData* data = scene.GetRenderObjectComponent(renderObject);
	if (data == nullptr || data->entity.index >= _drawItemCache.drawRangeByEntityIndex.size())
	{
		return;
	}

	JDrawRange& range = _drawItemCache.drawRangeByEntityIndex[data->entity.index];
	if (!range.valid || range.generation != data->entity.generation)
	{
		appendDrawItems(scene, renderObject, outResult);
		return;
	}

	if (!data->active || !data->visible || data->mesh == nullptr)
	{
		range.valid = false;
		return;
	}

	const JTransformHandle transform = scene.GetTransformHandle(data->entity);
	if (!transform.IsValid())
	{
		range.valid = false;
		return;
	}

	const std::vector<JMesh::SubMeshInfo>& subMeshes = data->mesh->GetSubMeshInfos();
	uint32 requiredCount = subMeshes.empty() ? 1 : 0;
	for (const JMesh::SubMeshInfo& subMesh : subMeshes)
	{
		if (subMesh.endIndex > subMesh.startIndex)
		{
			++requiredCount;
		}
	}
	if (requiredCount != range.count)
	{
		rebuildDrawItemCache(scene, outResult);
		return;
	}

	outResult.activeMeshes.insert(data->mesh);
	if (subMeshes.empty())
	{
		JDrawItem& drawItem = _drawItemCache.drawItems[range.start];
		drawItem.entity = data->entity;
		drawItem.renderObject = renderObject;
		drawItem.transform = transform;
		drawItem.material = data->material;
		drawItem.mesh = data->mesh;
		drawItem.subMeshIndex = 0;
		drawItem.indexCount = 0;
		drawItem.startIndex = 0;
		drawItem.transparent = data->transparent;
		return;
	}

	uint32 rangeOffset = 0;
	for (uint32 subMeshIndex = 0; subMeshIndex < subMeshes.size(); ++subMeshIndex)
	{
		const JMesh::SubMeshInfo& subMesh = subMeshes[subMeshIndex];
		if (subMesh.endIndex <= subMesh.startIndex)
		{
			continue;
		}

		JDrawItem& drawItem = _drawItemCache.drawItems[range.start + rangeOffset];
		drawItem.entity = data->entity;
		drawItem.renderObject = renderObject;
		drawItem.transform = transform;
		drawItem.material = resolveDrawMaterial(*data, subMesh);
		drawItem.mesh = data->mesh;
		drawItem.subMeshIndex = subMeshIndex;
		drawItem.indexCount = subMesh.endIndex - subMesh.startIndex;
		drawItem.startIndex = subMesh.startIndex;
		drawItem.transparent = data->transparent;
		++rangeOffset;
	}
}

void JRenderServer::SyncScene(JScene& scene)
{
	if (_syncedScene != &scene)
	{
		_drawItemCache = {};
		_syncedScene = &scene;
	}

	_primaryCamera = scene.GetPrimaryCamera();
	const std::vector<JCameraHandle> cameraHandles = collectSceneCameraHandles(scene);
	const std::vector<JSceneRenderObjectEvent> renderObjectEvents = scene.ConsumeRenderObjectEvents();
	scene.ConsumeDirtyCameras();
	scene.ConsumeDirtyLights();

	JRenderSnapshotBuilder::Input input;
	input.scene = &scene;
	input.cameras = &cameraHandles;

	JRenderSnapshotBuilder::Result buildResult;
	JRenderSnapshotBuilder::Build(input, _frameSnapshot, buildResult);
	updateDrawItemCache(scene, renderObjectEvents, buildResult);

	JCameraRenderQueueBuilder::Input queueInput;
	queueInput.drawItemCache = &_drawItemCache;
	queueInput.renderDB = nullptr;
	queueInput.scene = &scene;
	queueInput.jobSystem = _jobSystem;
	JCameraRenderQueueBuilder::Build(queueInput, _frameSnapshot);
}

bool JRenderServer::BuildFrameDesc(JRenderTarget* renderTarget, JRenderer::FrameDesc& outFrameDesc) const
{
	if (!_primaryCamera.IsValid() || renderTarget == nullptr)
	{
		return false;
	}

	outFrameDesc = {};
	outFrameDesc.camera = _primaryCamera;
	outFrameDesc.renderTarget = renderTarget;
	outFrameDesc.clearColor = renderTarget->GetClearColor();
	outFrameDesc.viewport = renderTarget->GetViewport();
	outFrameDesc.scissorRect = renderTarget->GetScissorRect();
	outFrameDesc.drawItemCache = &_drawItemCache;
	const JCameraSnapshot* cameraSnapshot = findCameraSnapshot(_primaryCamera);
	if (cameraSnapshot == nullptr)
	{
		return false;
	}

	outFrameDesc.cameraViewProjection = cameraSnapshot->viewProjection;
	outFrameDesc.cameraInverseViewProjection = cameraSnapshot->inverseViewProjection;
	outFrameDesc.cameraWorldPosition = cameraSnapshot->worldPosition;
	for (const JTransformSnapshot& snapshot : _frameSnapshot.transforms)
	{
		outFrameDesc.transformSnapshots.push_back({ snapshot.transform, snapshot.world });
	}
	if (_syncedScene != nullptr)
	{
		const std::vector<JScene::MaterialSlot>& materialSlots = _syncedScene->GetMaterialSlots();
		for (uint32 materialIndex : _syncedScene->GetActiveMaterialIndices())
		{
			if (materialIndex >= materialSlots.size())
			{
				continue;
			}

			const JScene::MaterialSlot& slot = materialSlots[materialIndex];
			if (slot.active && slot.material != nullptr)
			{
				outFrameDesc.materialSnapshots.push_back({ { materialIndex, slot.generation }, slot.material.get() });
			}
		}
	}
	if (!_frameSnapshot.lights.empty())
	{
		for (const JLightSnapshot::Item& item : _frameSnapshot.lights.front().items)
		{
			outFrameDesc.lightSnapshot.items.push_back({ item.colorIntensity, item.position });
		}
	}

	outFrameDesc.cullingTestedDrawItemCount = cameraSnapshot->cullingTestedDrawItemCount;
	outFrameDesc.culledDrawItemCount = cameraSnapshot->culledDrawItemCount;
	outFrameDesc.opaqueDrawItemIndices = cameraSnapshot->opaqueDrawItemIndices;
	outFrameDesc.transparentDrawItemIndices = cameraSnapshot->transparentDrawItemIndices;
	return true;
}

J_ENGINE_END
