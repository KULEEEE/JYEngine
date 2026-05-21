#include "engine/render/JRenderServer.h"

#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"
#include "engine/render/JRenderSnapshotBuilder.h"

J_ENGINE_BEGIN

uint64 JRenderServer::makeCameraKey(JCameraHandle camera)
{
	return (static_cast<uint64>(camera.generation) << 32) | camera.index;
}

JRenderServer::~JRenderServer()
{
}

uint32 JRenderServer::findMaterialIndex(uint32 materialID) const
{
	const auto iter = _materialIndexMap.find(materialID);
	return iter == _materialIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
}

JMaterial* JRenderServer::findMaterial(uint32 materialID) const
{
	const uint32 index = findMaterialIndex(materialID);
	return index == static_cast<uint32>(-1) ? nullptr : _materials[index].source;
}

uint32 JRenderServer::findCameraIndex(JCameraHandle camera) const
{
	const auto iter = _cameraIndexMap.find(makeCameraKey(camera));
	return iter == _cameraIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
}

JRenderServer::CameraRecord* JRenderServer::findCameraRecord(JCameraHandle camera)
{
	const uint32 index = findCameraIndex(camera);
	return index == static_cast<uint32>(-1) ? nullptr : &_cameras[index];
}

const JRenderServer::CameraRecord* JRenderServer::findCameraRecord(JCameraHandle camera) const
{
	const uint32 index = findCameraIndex(camera);
	return index == static_cast<uint32>(-1) ? nullptr : &_cameras[index];
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

void JRenderServer::RegisterMaterial(JMaterial* material)
{
	if (material == nullptr)
	{
		return;
	}

	if (findMaterialIndex(material->instanceID) != static_cast<uint32>(-1))
	{
		return;
	}

	const uint32 newIndex = static_cast<uint32>(_materials.size());
	_materials.push_back({ material->instanceID, material });
	_materialIndexMap[material->instanceID] = newIndex;
	MarkMaterialDirty(material);
}

void JRenderServer::UnregisterMaterial(uint32 materialID)
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

void JRenderServer::RegisterCamera(JCameraHandle camera)
{
	if (!camera.IsValid())
	{
		return;
	}

	if (findCameraIndex(camera) != static_cast<uint32>(-1))
	{
		MarkCameraDirty(camera);
		return;
	}

	const uint32 newIndex = static_cast<uint32>(_cameras.size());
	_cameras.push_back({ camera });
	_cameraIndexMap[makeCameraKey(camera)] = newIndex;
	MarkCameraDirty(camera);
}

void JRenderServer::UnregisterCamera(JCameraHandle camera)
{
	const uint32 index = findCameraIndex(camera);
	if (index != static_cast<uint32>(-1))
	{
		const uint32 lastIndex = static_cast<uint32>(_cameras.size() - 1);
		if (index != lastIndex)
		{
			_cameras[index] = _cameras[lastIndex];
			_cameraIndexMap[makeCameraKey(_cameras[index].camera)] = index;
		}
		_cameras.pop_back();
		_cameraIndexMap.erase(makeCameraKey(camera));
	}

	for (auto iter = _dirtyCameraKeys.begin(); iter != _dirtyCameraKeys.end();)
	{
		if (*iter == makeCameraKey(camera))
		{
			iter = _dirtyCameraKeys.erase(iter);
			continue;
		}
		++iter;
	}

	_renderDB.RemoveCameraResource(camera);
}

void JRenderServer::MarkMaterialDirty(JMaterial* material)
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

void JRenderServer::MarkCameraDirty(JCameraHandle camera)
{
	if (!camera.IsValid())
	{
		return;
	}

	const uint64 cameraKey = makeCameraKey(camera);
	for (uint64 dirtyCameraKey : _dirtyCameraKeys)
	{
		if (dirtyCameraKey == cameraKey)
		{
			return;
		}
	}

	_dirtyCameraKeys.push_back(cameraKey);
}

void JRenderServer::Sync()
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

void JRenderServer::syncCameraResources()
{
	for (const JCameraSnapshot& snapshot : _frameSnapshot.cameras)
	{
		_renderDB.SyncCamera(snapshot.camera, snapshot.viewProjection);
	}
	_dirtyCameraKeys.clear();
}

std::vector<JCameraHandle> JRenderServer::collectRegisteredCameraHandles() const
{
	std::vector<JCameraHandle> cameras;
	cameras.reserve(_cameras.size());
	for (const CameraRecord& record : _cameras)
	{
		cameras.push_back(record.camera);
	}
	return cameras;
}

void JRenderServer::syncTransformResources()
{
	for (const JTransformSnapshot& snapshot : _frameSnapshot.transforms)
	{
		_renderDB.SyncTransform(snapshot.transform, snapshot.world);
	}
}

void JRenderServer::syncLightResources()
{
	for (const JLightSnapshot& snapshot : _frameSnapshot.lights)
	{
		_renderDB.SyncLight(snapshot);
	}
}

void JRenderServer::ensureMeshResources(const std::unordered_set<const JMesh*>& activeMeshes)
{
	for (const JMesh* mesh : activeMeshes)
	{
		_renderDB.GetOrCreateMeshResource(mesh);
	}
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

		const DrawRange& range = _drawItemCache.drawRangeByEntityIndex[entityIndex];
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
	for (uint32 renderObjectIndex = 0; renderObjectIndex < slots.size(); ++renderObjectIndex)
	{
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

	DrawRange& existingRange = _drawItemCache.drawRangeByEntityIndex[data->entity.index];
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
		drawItem.materialID = data->materialID;
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
			drawItem.materialID = data->materialID;
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

	DrawRange& range = _drawItemCache.drawRangeByEntityIndex[data->entity.index];
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

	DrawRange& range = _drawItemCache.drawRangeByEntityIndex[data->entity.index];
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
		drawItem.materialID = data->materialID;
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
		drawItem.materialID = data->materialID;
		drawItem.mesh = data->mesh;
		drawItem.subMeshIndex = subMeshIndex;
		drawItem.indexCount = subMesh.endIndex - subMesh.startIndex;
		drawItem.startIndex = subMesh.startIndex;
		drawItem.transparent = data->transparent;
		++rangeOffset;
	}
}

void JRenderServer::buildCameraDrawItemIndices()
{
	for (JCameraSnapshot& cameraSnapshot : _frameSnapshot.cameras)
	{
		cameraSnapshot.opaqueDrawItemIndices.clear();
		cameraSnapshot.transparentDrawItemIndices.clear();
		for (uint32 entityIndex : _drawItemCache.activeDrawEntityIndices)
		{
			if (entityIndex >= _drawItemCache.drawRangeByEntityIndex.size())
			{
				continue;
			}

			const DrawRange& range = _drawItemCache.drawRangeByEntityIndex[entityIndex];
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
				if (drawItem.transparent)
				{
					cameraSnapshot.transparentDrawItemIndices.push_back(drawItemIndex);
				}
				else
				{
					cameraSnapshot.opaqueDrawItemIndices.push_back(drawItemIndex);
				}
			}
		}
	}
}

void JRenderServer::resolveDrawItemResources()
{
	for (JDrawItem& drawItem : _drawItemCache.drawItems)
	{
		drawItem.meshResource = _renderDB.FindMeshResource(drawItem.mesh);
		drawItem.materialResource = _renderDB.FindMaterialResource(drawItem.materialID);
		drawItem.transformResource = _renderDB.FindTransformResource(drawItem.transform);
		if (drawItem.indexCount == 0 && drawItem.meshResource != nullptr)
		{
			drawItem.indexCount = static_cast<uint32>(drawItem.meshResource->indexSize);
		}
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

	const std::vector<JCameraHandle> cameraHandles = collectRegisteredCameraHandles();
	const std::vector<JSceneRenderObjectEvent> renderObjectEvents = scene.ConsumeRenderObjectEvents();
	JRenderSnapshotBuilder::Input input;
	input.scene = &scene;
	input.cameras = &cameraHandles;

	JRenderSnapshotBuilder::Result buildResult;
	JRenderSnapshotBuilder::Build(input, _frameSnapshot, buildResult);
	updateDrawItemCache(scene, renderObjectEvents, buildResult);
	buildCameraDrawItemIndices();

	syncCameraResources();
	syncTransformResources();
	syncLightResources();
	ensureMeshResources(buildResult.activeMeshes);
	resolveDrawItemResources();

	_renderDB.PruneUnusedSceneResources(buildResult.activeCameraKeys, buildResult.activeTransformKeys, buildResult.activeMeshes);
}

bool JRenderServer::BuildFrameDesc(JRenderTarget* renderTarget, const JColor& clearColor, const Render::JViewport& viewport, const D3D12_RECT& scissorRect, JRenderer::FrameDesc& outFrameDesc) const
{
	if (!_primaryCamera.IsValid())
	{
		return false;
	}

	outFrameDesc = {};
	outFrameDesc.camera = _primaryCamera;
	outFrameDesc.renderTarget = renderTarget;
	outFrameDesc.clearColor = clearColor;
	outFrameDesc.viewport = viewport;
	outFrameDesc.scissorRect = scissorRect;
	const JCameraSnapshot* cameraSnapshot = findCameraSnapshot(_primaryCamera);
	if (cameraSnapshot == nullptr)
	{
		return false;
	}

	outFrameDesc.opaqueDrawItems.reserve(cameraSnapshot->opaqueDrawItemIndices.size());
	for (uint32 drawItemIndex : cameraSnapshot->opaqueDrawItemIndices)
	{
		if (drawItemIndex < _drawItemCache.drawItems.size())
		{
			outFrameDesc.opaqueDrawItems.push_back(_drawItemCache.drawItems[drawItemIndex]);
		}
	}

	outFrameDesc.transparentDrawItems.reserve(cameraSnapshot->transparentDrawItemIndices.size());
	for (uint32 drawItemIndex : cameraSnapshot->transparentDrawItemIndices)
	{
		if (drawItemIndex < _drawItemCache.drawItems.size())
		{
			outFrameDesc.transparentDrawItems.push_back(_drawItemCache.drawItems[drawItemIndex]);
		}
	}

	return true;
}

J_ENGINE_END
