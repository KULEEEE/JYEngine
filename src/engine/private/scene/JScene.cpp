#include "engine/scene/JScene.h"

#include "engine/asset/JMaterial.h"

#include <iomanip>
#include <sstream>

J_ENGINE_BEGIN

namespace
{
	// stableID 문자열을 빠르게 찾기 위한 해시.
	// 충돌 가능성은 있으므로 최종 반환 전 원문 문자열을 한 번 더 비교한다.
	uint64 makeStableIDHash(const std::string& stableID)
	{
		constexpr uint64 FNV_OFFSET = 14695981039346656037ull;
		constexpr uint64 FNV_PRIME = 1099511628211ull;

		uint64 hash = FNV_OFFSET;
		for (unsigned char ch : stableID)
		{
			hash ^= ch;
			hash *= FNV_PRIME;
		}
		return hash;
	}
}

std::string JScene::generateStableID()
{
	for (;;)
	{
		std::ostringstream stream;
		stream << "ent_" << std::setw(6) << std::setfill('0') << _nextStableID++;
		const std::string stableID = stream.str();
		if (!FindEntityByStableID(stableID).IsValid())
		{
			return stableID;
		}
	}
}

JEntityHandle JScene::CreateEntity(const std::string& stableID, const std::string& name, const std::vector<std::string>& tags)
{
	if (!stableID.empty() && FindEntityByStableID(stableID).IsValid())
	{
		return {};
	}

	EntityData data;
	data.active = true;
	const JEntityHandle entity = _entities.Add(data);

	// Entity pool 슬롯 수에 맞춰 메타데이터/lookup 배열도 같은 index로 맞춘다.
	_entityMetadata.resize(_entities.GetSlots().size());
	_entityTransformLookup.resize(_entities.GetSlots().size());

	const std::string resolvedStableID = stableID.empty() ? generateStableID() : stableID;
	SetEntityMetadata(entity, resolvedStableID, name, tags);
	return entity;
}

JMaterialHandle JScene::AddMaterial(std::shared_ptr<JMaterial> material)
{
	if (material == nullptr)
	{
		return {};
	}

	if (!_freeMaterialIndices.empty())
	{
		const uint32 index = _freeMaterialIndices.back();
		_freeMaterialIndices.pop_back();

		MaterialSlot& slot = _materials[index];
		slot.active = true;
		slot.material = std::move(material);
		_activeMaterialIndices.push_back(index);
		return { index, slot.generation };
	}

	MaterialSlot slot;
	slot.active = true;
	slot.material = std::move(material);
	_materials.push_back(std::move(slot));
	const uint32 index = static_cast<uint32>(_materials.size() - 1);
	_activeMaterialIndices.push_back(index);
	return { index, _materials[index].generation };
}

bool JScene::RemoveMaterial(JMaterialHandle material)
{
	if (!material.IsValid()
		|| material.index >= _materials.size()
		|| !_materials[material.index].active
		|| _materials[material.index].generation != material.generation)
	{
		return false;
	}

	MaterialSlot& slot = _materials[material.index];
	slot.active = false;
	slot.material.reset();

	// 같은 index가 재사용되더라도 이전 handle이 유효하지 않도록 generation을 올린다.
	++slot.generation;
	_freeMaterialIndices.push_back(material.index);
	for (uint32 i = 0; i < _activeMaterialIndices.size(); ++i)
	{
		if (_activeMaterialIndices[i] == material.index)
		{
			_activeMaterialIndices[i] = _activeMaterialIndices.back();
			_activeMaterialIndices.pop_back();
			break;
		}
	}
	return true;
}

bool JScene::RemoveEntity(JEntityHandle entity)
{
	if (!_entities.IsValid(entity))
	{
		return false;
	}

	// Entity 제거 전에 연결된 컴포넌트를 먼저 제거해서 각 pool의 generation을 갱신한다.
	RemoveRenderObjectComponent(entity);
	RemoveLight(entity);
	RemoveCamera(entity);
	RemoveTransform(entity);

	if (entity.index < _entityMetadata.size())
	{
		JEntityMetadata& metadata = _entityMetadata[entity.index];
		if (metadata.stableIDHash != 0)
		{
			_stableIDLookup.erase(metadata.stableIDHash);
		}
		metadata = {};
	}

	if (entity.index < _entityTransformLookup.size())
	{
		_entityTransformLookup[entity.index] = {};
	}

	return _entities.Remove(entity);
}

bool JScene::SetEntityMetadata(JEntityHandle entity, const std::string& stableID, const std::string& name, const std::vector<std::string>& tags)
{
	if (!_entities.IsValid(entity))
	{
		return false;
	}

	const std::string resolvedStableID = stableID.empty() ? generateStableID() : stableID;
	const uint64 stableIDHash = makeStableIDHash(resolvedStableID);
	const auto existingIter = _stableIDLookup.find(stableIDHash);
	if (existingIter != _stableIDLookup.end()
		&& (existingIter->second.index != entity.index || existingIter->second.generation != entity.generation))
	{
		return false;
	}

	if (entity.index >= _entityMetadata.size())
	{
		_entityMetadata.resize(entity.index + 1);
	}

	JEntityMetadata& metadata = _entityMetadata[entity.index];
	if (metadata.stableIDHash != 0 && metadata.stableIDHash != stableIDHash)
	{
		_stableIDLookup.erase(metadata.stableIDHash);
	}

	metadata.stableIDHash = stableIDHash;
	metadata.stableID = resolvedStableID;
	metadata.name = name;
	metadata.tags = tags;
	_stableIDLookup[stableIDHash] = entity;
	return true;
}

void JScene::SetEntityName(JEntityHandle entity, const std::string& name)
{
	JEntityMetadata* metadata = GetEntityMetadata(entity);
	if (metadata != nullptr)
	{
		metadata->name = name;
	}
}

const JEntityMetadata* JScene::GetEntityMetadata(JEntityHandle entity) const
{
	return _entities.IsValid(entity) && entity.index < _entityMetadata.size()
		? &_entityMetadata[entity.index]
		: nullptr;
}

JEntityMetadata* JScene::GetEntityMetadata(JEntityHandle entity)
{
	return _entities.IsValid(entity) && entity.index < _entityMetadata.size()
		? &_entityMetadata[entity.index]
		: nullptr;
}

JEntityHandle JScene::FindEntityByStableID(const std::string& stableID) const
{
	if (stableID.empty())
	{
		return {};
	}

	const uint64 stableIDHash = makeStableIDHash(stableID);
	const auto iter = _stableIDLookup.find(stableIDHash);
	if (iter == _stableIDLookup.end())
	{
		return {};
	}

	const JEntityMetadata* metadata = GetEntityMetadata(iter->second);
	// 해시 충돌 또는 stale handle 방지를 위해 원문 stableID까지 확인한다.
	return metadata != nullptr && metadata->stableID == stableID ? iter->second : JEntityHandle{};
}

void JScene::addEntityComponentMask(JEntityHandle entity, JSceneComponentMask component)
{
	JEntityMetadata* metadata = GetEntityMetadata(entity);
	if (metadata != nullptr)
	{
		metadata->componentMask |= static_cast<uint32>(component);
	}
}

void JScene::removeEntityComponentMask(JEntityHandle entity, JSceneComponentMask component)
{
	JEntityMetadata* metadata = GetEntityMetadata(entity);
	if (metadata != nullptr)
	{
		metadata->componentMask &= ~static_cast<uint32>(component);
	}
}

void JScene::pushRenderObjectEvent(JSceneRenderObjectEventType type, JRenderObjectComponentHandle handle, JEntityHandle entity)
{
	_renderObjectEvents.push_back({ type, handle, entity });
}

void JScene::markCameraDirtyForEntity(JEntityHandle entity)
{
	const JCameraHandle camera = GetCameraHandle(entity);
	if (camera.IsValid())
	{
		MarkCameraDirty(camera);
	}
}

void JScene::markLightDirtyForEntity(JEntityHandle entity)
{
	const JLightHandle light = GetLightHandle(entity);
	if (light.IsValid())
	{
		MarkLightDirty(light);
	}
}

JTransformHandle JScene::AddTransform(JEntityHandle entity, const TransformData& data)
{
	if (!_entities.IsValid(entity))
	{
		return {};
	}

	const JTransformHandle transform = _transforms.Add(entity, data);
	if (!transform.IsValid())
	{
		return {};
	}

	if (entity.index >= _entityTransformLookup.size())
	{
		_entityTransformLookup.resize(entity.index + 1);
	}

	// Transform은 entity index와 transform index가 항상 같다고 가정하지 않고 lookup으로 찾는다.
	_entityTransformLookup[entity.index] = transform;
	addEntityComponentMask(entity, JSceneComponentMask::Transform);
	return transform;
}

JCameraHandle JScene::AddCamera(JEntityHandle entity, JTransformHandle transform, float aspectRatio, float nearP, float farP)
{
	if (!_entities.IsValid(entity) || !_transforms.IsValid(transform)
		|| transform.index != entity.index || transform.generation != entity.generation)
	{
		return {};
	}

	CameraData data;
	data.entity = entity;
	data.aspectRatio = aspectRatio;
	data.nearP = nearP;
	data.farP = farP;

	const JCameraHandle handle = _cameras.Add(entity, data);
	if (!handle.IsValid())
	{
		return {};
	}

	if (!_primaryCamera.IsValid())
	{
		_primaryCamera = handle;
	}
	addEntityComponentMask(entity, JSceneComponentMask::Camera);

	// 카메라 추가/수정은 renderer의 view/projection 캐시를 갱신해야 한다.
	MarkCameraDirty(handle);
	return handle;
}

JCameraHandle JScene::AddCamera(JEntityHandle entity, float aspectRatio, float nearP, float farP)
{
	const JTransformHandle transform = GetTransformHandle(entity);
	return AddCamera(entity, transform, aspectRatio, nearP, farP);
}

JLightHandle JScene::AddLight(JEntityHandle entity, const LightData& data)
{
	if (!_entities.IsValid(entity) || !GetTransformHandle(entity).IsValid())
	{
		return {};
	}

	LightData lightData = data;
	lightData.entity = entity;
	const JLightHandle light = _lights.Add(entity, lightData);
	if (!light.IsValid())
	{
		return {};
	}

	addEntityComponentMask(entity, JSceneComponentMask::Light);

	// 조명 데이터는 렌더 패스에서 별도 버퍼로 모으므로 변경 표시를 남긴다.
	MarkLightDirty(light);
	return light;
}

JRenderObjectComponentHandle JScene::AddRenderObjectComponent(JEntityHandle entity, JMaterialHandle material, const JMesh* mesh, bool transparent)
{
	return AddRenderObjectComponent(entity, material, {}, mesh, transparent);
}

JRenderObjectComponentHandle JScene::AddRenderObjectComponent(JEntityHandle entity, JMaterialHandle material, const std::vector<JMaterialHandle>& subMeshMaterials, const JMesh* mesh, bool transparent)
{
	if (!_entities.IsValid(entity) || !GetTransformHandle(entity).IsValid() || GetMaterial(material) == nullptr)
	{
		return {};
	}

	for (JMaterialHandle subMeshMaterial : subMeshMaterials)
	{
		if (subMeshMaterial.IsValid() && GetMaterial(subMeshMaterial) == nullptr)
		{
			return {};
		}
	}

	RenderObjectComponentData data;
	data.entity = entity;
	data.mesh = mesh;
	data.material = material;
	data.subMeshMaterials = subMeshMaterials;
	data.transparent = transparent;
	const JRenderObjectComponentHandle renderObject = _renderObjectComponents.Add(entity, data);
	if (!renderObject.IsValid())
	{
		return {};
	}

	addEntityComponentMask(entity, JSceneComponentMask::RenderObject);

	// RenderObject 변경은 RenderDB/draw item cache 쪽에서 증분 반영한다.
	pushRenderObjectEvent(JSceneRenderObjectEventType::Added, renderObject, entity);
	return renderObject;
}

bool JScene::RemoveTransform(JTransformHandle transform)
{
	if (!_transforms.IsValid(transform))
	{
		return false;
	}

	const JEntityHandle entity = _transforms.GetSlots()[transform.index].entity;
	if (!_transforms.Remove(transform))
	{
		return false;
	}

	removeEntityComponentMask(entity, JSceneComponentMask::Transform);
	if (entity.index < _entityTransformLookup.size())
	{
		_entityTransformLookup[entity.index] = {};
	}
	return true;
}

bool JScene::RemoveTransform(JEntityHandle entity)
{
	return RemoveTransform(GetTransformHandle(entity));
}

bool JScene::RemoveCamera(JCameraHandle camera)
{
	CameraData* cameraData = _cameras.Get(camera);
	if (cameraData == nullptr)
	{
		return false;
	}

	const JEntityHandle entity = cameraData->entity;
	if (!_cameras.Remove(camera))
	{
		return false;
	}

	removeEntityComponentMask(entity, JSceneComponentMask::Camera);
	if (_primaryCamera.index == camera.index && _primaryCamera.generation == camera.generation)
	{
		_primaryCamera = {};
	}
	return true;
}

bool JScene::RemoveCamera(JEntityHandle entity)
{
	return RemoveCamera(GetCameraHandle(entity));
}

bool JScene::RemoveLight(JLightHandle light)
{
	LightData* lightData = _lights.Get(light);
	if (lightData == nullptr)
	{
		return false;
	}

	const JEntityHandle entity = lightData->entity;
	MarkLightDirty(light);
	if (!_lights.Remove(light))
	{
		return false;
	}

	removeEntityComponentMask(entity, JSceneComponentMask::Light);
	return true;
}

bool JScene::RemoveLight(JEntityHandle entity)
{
	return RemoveLight(GetLightHandle(entity));
}

bool JScene::RemoveRenderObjectComponent(JRenderObjectComponentHandle handle)
{
	RenderObjectComponentData* renderObject = _renderObjectComponents.Get(handle);
	if (renderObject == nullptr)
	{
		return false;
	}

	const JEntityHandle entity = renderObject->entity;
	if (!_renderObjectComponents.Remove(handle))
	{
		return false;
	}

	removeEntityComponentMask(entity, JSceneComponentMask::RenderObject);
	pushRenderObjectEvent(JSceneRenderObjectEventType::Removed, handle, entity);
	return true;
}

bool JScene::RemoveRenderObjectComponent(JEntityHandle entity)
{
	return RemoveRenderObjectComponent(GetRenderObjectComponentHandle(entity));
}

JScene::EntityData* JScene::GetEntity(JEntityHandle handle)
{
	return _entities.Get(handle);
}

const JScene::EntityData* JScene::GetEntity(JEntityHandle handle) const
{
	return _entities.Get(handle);
}

JMaterial* JScene::GetMaterial(JMaterialHandle handle)
{
	return handle.IsValid()
		&& handle.index < _materials.size()
		&& _materials[handle.index].active
		&& _materials[handle.index].generation == handle.generation
		? _materials[handle.index].material.get()
		: nullptr;
}

const JMaterial* JScene::GetMaterial(JMaterialHandle handle) const
{
	return handle.IsValid()
		&& handle.index < _materials.size()
		&& _materials[handle.index].active
		&& _materials[handle.index].generation == handle.generation
		? _materials[handle.index].material.get()
		: nullptr;
}

JScene::TransformData JScene::GetTransform(JTransformHandle handle) const
{
	return _transforms.Get(handle);
}

JScene::TransformData JScene::GetTransform(JEntityHandle entity) const
{
	if (!_entities.IsValid(entity) || entity.index >= _entityTransformLookup.size())
	{
		return {};
	}

	const JTransformHandle transform = _entityTransformLookup[entity.index];
	return _transforms.Get(transform);
}

void JScene::SetTransform(JTransformHandle handle, const TransformData& data)
{
	_transforms.Set(handle, data);
	if (_transforms.IsValid(handle))
	{
		const JEntityHandle entity = _transforms.GetSlots()[handle.index].entity;

		// Transform이 바뀌면 같은 entity의 camera/light 파생 데이터도 다시 계산해야 한다.
		markCameraDirtyForEntity(entity);
		markLightDirtyForEntity(entity);
	}
}

void JScene::SetTransform(JEntityHandle entity, const TransformData& data)
{
	if (JTransformHandle transform = GetTransformHandle(entity); transform.IsValid())
	{
		_transforms.Set(transform, data);
	}
}

void JScene::SetTransformTranslation(JTransformHandle handle, const JVec3& value)
{
	_transforms.SetTranslation(handle, value);
	if (_transforms.IsValid(handle))
	{
		const JEntityHandle entity = _transforms.GetSlots()[handle.index].entity;
		markCameraDirtyForEntity(entity);
		markLightDirtyForEntity(entity);
	}
}

void JScene::SetTransformTranslation(JEntityHandle entity, const JVec3& value)
{
	if (JTransformHandle transform = GetTransformHandle(entity); transform.IsValid())
	{
		_transforms.SetTranslation(transform, value);
	}
}

void JScene::SetTransformRotation(JTransformHandle handle, const JVec3& value)
{
	_transforms.SetRotation(handle, value);
	if (_transforms.IsValid(handle))
	{
		const JEntityHandle entity = _transforms.GetSlots()[handle.index].entity;
		markCameraDirtyForEntity(entity);
		markLightDirtyForEntity(entity);
	}
}

void JScene::SetTransformRotation(JEntityHandle entity, const JVec3& value)
{
	if (JTransformHandle transform = GetTransformHandle(entity); transform.IsValid())
	{
		_transforms.SetRotation(transform, value);
	}
}

void JScene::SetTransformScale(JTransformHandle handle, const JVec3& value)
{
	_transforms.SetScale(handle, value);
	if (_transforms.IsValid(handle))
	{
		const JEntityHandle entity = _transforms.GetSlots()[handle.index].entity;
		markCameraDirtyForEntity(entity);
		markLightDirtyForEntity(entity);
	}
}

void JScene::SetTransformScale(JEntityHandle entity, const JVec3& value)
{
	if (JTransformHandle transform = GetTransformHandle(entity); transform.IsValid())
	{
		_transforms.SetScale(transform, value);
	}
}

std::vector<uint32> JScene::ConsumeDirtyTransformIndices()
{
	return _transforms.ConsumeDirtyIndices();
}

bool JScene::HasDirtyTransforms() const
{
	return _transforms.HasDirty();
}

const JVec3* JScene::GetTransformTranslation(JTransformHandle handle) const
{
	return _transforms.GetTranslation(handle);
}

const JVec3* JScene::GetTransformTranslation(JEntityHandle entity) const
{
	return _entities.IsValid(entity) && entity.index < _entityTransformLookup.size()
		? _transforms.GetTranslation(_entityTransformLookup[entity.index])
		: nullptr;
}

const JVec3* JScene::GetTransformRotation(JTransformHandle handle) const
{
	return _transforms.GetRotation(handle);
}

const JVec3* JScene::GetTransformRotation(JEntityHandle entity) const
{
	return _entities.IsValid(entity) && entity.index < _entityTransformLookup.size()
		? _transforms.GetRotation(_entityTransformLookup[entity.index])
		: nullptr;
}

const JVec3* JScene::GetTransformScale(JTransformHandle handle) const
{
	return _transforms.GetScale(handle);
}

const JVec3* JScene::GetTransformScale(JEntityHandle entity) const
{
	return _entities.IsValid(entity) && entity.index < _entityTransformLookup.size()
		? _transforms.GetScale(_entityTransformLookup[entity.index])
		: nullptr;
}

JTransformHandle JScene::GetTransformHandle(JEntityHandle entity) const
{
	if (!_entities.IsValid(entity) || entity.index >= _entityTransformLookup.size())
	{
		return {};
	}

	const JTransformHandle transform = _entityTransformLookup[entity.index];
	return _transforms.IsValid(transform) ? transform : JTransformHandle{};
}

JCameraHandle JScene::GetCameraHandle(JEntityHandle entity) const
{
	if (!_entities.IsValid(entity))
	{
		return {};
	}

	const JCameraHandle camera = { entity.index, entity.generation };
	return _cameras.IsValid(camera) ? camera : JCameraHandle{};
}

JLightHandle JScene::GetLightHandle(JEntityHandle entity) const
{
	if (!_entities.IsValid(entity))
	{
		return {};
	}

	const JLightHandle light = { entity.index, entity.generation };
	return _lights.IsValid(light) ? light : JLightHandle{};
}

JRenderObjectComponentHandle JScene::GetRenderObjectComponentHandle(JEntityHandle entity) const
{
	if (!_entities.IsValid(entity))
	{
		return {};
	}

	const JRenderObjectComponentHandle renderObject = { entity.index, entity.generation };
	return _renderObjectComponents.IsValid(renderObject) ? renderObject : JRenderObjectComponentHandle{};
}

JScene::CameraData* JScene::GetCamera(JCameraHandle handle)
{
	return _cameras.Get(handle);
}

const JScene::CameraData* JScene::GetCamera(JCameraHandle handle) const
{
	return _cameras.Get(handle);
}

JScene::CameraData* JScene::GetCamera(JEntityHandle entity)
{
	return GetCamera(GetCameraHandle(entity));
}

const JScene::CameraData* JScene::GetCamera(JEntityHandle entity) const
{
	return GetCamera(GetCameraHandle(entity));
}

JScene::LightData* JScene::GetLight(JLightHandle handle)
{
	return _lights.Get(handle);
}

const JScene::LightData* JScene::GetLight(JLightHandle handle) const
{
	return _lights.Get(handle);
}

JScene::LightData* JScene::GetLight(JEntityHandle entity)
{
	return GetLight(GetLightHandle(entity));
}

const JScene::LightData* JScene::GetLight(JEntityHandle entity) const
{
	return GetLight(GetLightHandle(entity));
}

JScene::RenderObjectComponentData* JScene::GetRenderObjectComponent(JRenderObjectComponentHandle handle)
{
	return _renderObjectComponents.Get(handle);
}

const JScene::RenderObjectComponentData* JScene::GetRenderObjectComponent(JRenderObjectComponentHandle handle) const
{
	return _renderObjectComponents.Get(handle);
}

JScene::RenderObjectComponentData* JScene::GetRenderObjectComponent(JEntityHandle entity)
{
	return GetRenderObjectComponent(GetRenderObjectComponentHandle(entity));
}

const JScene::RenderObjectComponentData* JScene::GetRenderObjectComponent(JEntityHandle entity) const
{
	return GetRenderObjectComponent(GetRenderObjectComponentHandle(entity));
}

void JScene::SetCameraData(JCameraHandle camera, const CameraData& data)
{
	CameraData* cameraData = _cameras.Get(camera);
	if (cameraData == nullptr)
	{
		return;
	}

	CameraData resolved = data;
	resolved.entity = cameraData->entity;
	*cameraData = resolved;
	MarkCameraDirty(camera);
}

void JScene::SetCameraAspectRatio(JCameraHandle camera, float aspectRatio)
{
	CameraData* cameraData = _cameras.Get(camera);
	if (cameraData == nullptr)
	{
		return;
	}

	cameraData->aspectRatio = aspectRatio;
	MarkCameraDirty(camera);
}

void JScene::SetLightData(JLightHandle light, const LightData& data)
{
	LightData* lightData = _lights.Get(light);
	if (lightData == nullptr)
	{
		return;
	}

	LightData resolved = data;
	resolved.entity = lightData->entity;
	*lightData = resolved;
	MarkLightDirty(light);
}

void JScene::SetRenderObjectComponentData(JRenderObjectComponentHandle handle, const RenderObjectComponentData& data)
{
	RenderObjectComponentData* renderObject = _renderObjectComponents.Get(handle);
	if (renderObject == nullptr)
	{
		return;
	}

	RenderObjectComponentData resolved = data;
	resolved.entity = renderObject->entity;
	*renderObject = resolved;

	// mesh/material/visibility 계열 변경은 draw item 재구성이 필요할 수 있다.
	pushRenderObjectEvent(JSceneRenderObjectEventType::Modified, handle, resolved.entity);
}

void JScene::SetRenderObjectVisible(JRenderObjectComponentHandle handle, bool visible)
{
	RenderObjectComponentData* renderObject = _renderObjectComponents.Get(handle);
	if (renderObject == nullptr || renderObject->visible == visible)
	{
		return;
	}

	renderObject->visible = visible;
	pushRenderObjectEvent(JSceneRenderObjectEventType::Modified, handle, renderObject->entity);
}

void JScene::SetRenderObjectVisible(JEntityHandle entity, bool visible)
{
	SetRenderObjectVisible(GetRenderObjectComponentHandle(entity), visible);
}

void JScene::MarkCameraDirty(JCameraHandle camera)
{
	if (!_cameras.IsValid(camera))
	{
		return;
	}

	// 같은 프레임 안에서 중복 갱신 요청이 쌓이지 않도록 한 번만 넣는다.
	for (JCameraHandle dirtyCamera : _dirtyCameras)
	{
		if (dirtyCamera.index == camera.index && dirtyCamera.generation == camera.generation)
		{
			return;
		}
	}

	_dirtyCameras.push_back(camera);
}

void JScene::MarkCameraDirty(JEntityHandle entity)
{
	MarkCameraDirty(GetCameraHandle(entity));
}

std::vector<JCameraHandle> JScene::ConsumeDirtyCameras()
{
	std::vector<JCameraHandle> dirtyCameras = std::move(_dirtyCameras);
	_dirtyCameras.clear();

	// 소비 시점에 이미 삭제된 카메라는 제외한다.
	std::vector<JCameraHandle> validCameras;
	validCameras.reserve(dirtyCameras.size());
	for (JCameraHandle camera : dirtyCameras)
	{
		if (_cameras.IsValid(camera))
		{
			validCameras.push_back(camera);
		}
	}
	return validCameras;
}

void JScene::MarkLightDirty(JLightHandle light)
{
	if (!_lights.IsValid(light))
	{
		return;
	}

	// 같은 프레임 안에서 중복 갱신 요청이 쌓이지 않도록 한 번만 넣는다.
	for (JLightHandle dirtyLight : _dirtyLights)
	{
		if (dirtyLight.index == light.index && dirtyLight.generation == light.generation)
		{
			return;
		}
	}

	_dirtyLights.push_back(light);
}

void JScene::MarkLightDirty(JEntityHandle entity)
{
	MarkLightDirty(GetLightHandle(entity));
}

std::vector<JLightHandle> JScene::ConsumeDirtyLights()
{
	std::vector<JLightHandle> dirtyLights = std::move(_dirtyLights);
	_dirtyLights.clear();
	return dirtyLights;
}

void JScene::MarkRenderObjectComponentModified(JRenderObjectComponentHandle handle)
{
	const RenderObjectComponentData* renderObject = _renderObjectComponents.Get(handle);
	if (renderObject == nullptr)
	{
		return;
	}

	pushRenderObjectEvent(JSceneRenderObjectEventType::Modified, handle, renderObject->entity);
}

void JScene::MarkRenderObjectComponentModified(JEntityHandle entity)
{
	MarkRenderObjectComponentModified(GetRenderObjectComponentHandle(entity));
}

std::vector<JSceneRenderObjectEvent> JScene::ConsumeRenderObjectEvents()
{
	std::vector<JSceneRenderObjectEvent> events = std::move(_renderObjectEvents);
	_renderObjectEvents.clear();
	return events;
}

J_ENGINE_END
