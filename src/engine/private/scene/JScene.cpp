#include "engine/scene/JScene.h"

#include <iomanip>
#include <sstream>

J_ENGINE_BEGIN

namespace
{
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
	_entityMetadata.resize(_entities.GetSlots().size());
	_entityTransformLookup.resize(_entities.GetSlots().size());

	const std::string resolvedStableID = stableID.empty() ? generateStableID() : stableID;
	SetEntityMetadata(entity, resolvedStableID, name, tags);
	return entity;
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

JTransformHandle JScene::AddTransform(JEntityHandle entity, const TransformData& data)
{
	if (!_entities.IsValid(entity))
	{
		return {};
	}

	const JTransformHandle transform = _transforms.Add(entity, data);
	if (entity.index >= _entityTransformLookup.size())
	{
		_entityTransformLookup.resize(entity.index + 1);
	}
	_entityTransformLookup[entity.index] = transform;
	addEntityComponentMask(entity, JSceneComponentMask::Transform);
	return transform;
}

JCameraHandle JScene::AddCamera(JEntityHandle entity, JTransformHandle transform, float aspectRatio, float nearP, float farP)
{
	if (!_entities.IsValid(entity) || !_transforms.IsValid(transform))
	{
		return {};
	}

	CameraData data;
	data.entity = entity;
	data.aspectRatio = aspectRatio;
	data.nearP = nearP;
	data.farP = farP;

	const JCameraHandle handle = _cameras.Add(entity, data);
	if (!_primaryCamera.IsValid())
	{
		_primaryCamera = handle;
	}
	addEntityComponentMask(entity, JSceneComponentMask::Camera);
	return handle;
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
	addEntityComponentMask(entity, JSceneComponentMask::Light);
	return light;
}

JRenderObjectComponentHandle JScene::AddRenderObjectComponent(JEntityHandle entity, uint32 materialID, const JMesh* mesh, bool transparent)
{
	if (!_entities.IsValid(entity) || !GetTransformHandle(entity).IsValid())
	{
		return {};
	}

	RenderObjectComponentData data;
	data.entity = entity;
	data.mesh = mesh;
	data.materialID = materialID;
	data.transparent = transparent;
	const JRenderObjectComponentHandle renderObject = _renderObjectComponents.Add(entity, data);
	addEntityComponentMask(entity, JSceneComponentMask::RenderObject);
	pushRenderObjectEvent(JSceneRenderObjectEventType::Added, renderObject, entity);
	return renderObject;
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

JScene::EntityData* JScene::GetEntity(JEntityHandle handle)
{
	return _entities.Get(handle);
}

const JScene::EntityData* JScene::GetEntity(JEntityHandle handle) const
{
	return _entities.Get(handle);
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

JVec3* JScene::GetTransformTranslation(JTransformHandle handle)
{
	return _transforms.GetTranslation(handle);
}

const JVec3* JScene::GetTransformTranslation(JTransformHandle handle) const
{
	return _transforms.GetTranslation(handle);
}

JVec3* JScene::GetTransformTranslation(JEntityHandle entity)
{
	return _entities.IsValid(entity) && entity.index < _entityTransformLookup.size()
		? _transforms.GetTranslation(_entityTransformLookup[entity.index])
		: nullptr;
}

const JVec3* JScene::GetTransformTranslation(JEntityHandle entity) const
{
	return _entities.IsValid(entity) && entity.index < _entityTransformLookup.size()
		? _transforms.GetTranslation(_entityTransformLookup[entity.index])
		: nullptr;
}

JVec3* JScene::GetTransformRotation(JTransformHandle handle)
{
	return _transforms.GetRotation(handle);
}

const JVec3* JScene::GetTransformRotation(JTransformHandle handle) const
{
	return _transforms.GetRotation(handle);
}

JVec3* JScene::GetTransformRotation(JEntityHandle entity)
{
	return _entities.IsValid(entity) && entity.index < _entityTransformLookup.size()
		? _transforms.GetRotation(_entityTransformLookup[entity.index])
		: nullptr;
}

const JVec3* JScene::GetTransformRotation(JEntityHandle entity) const
{
	return _entities.IsValid(entity) && entity.index < _entityTransformLookup.size()
		? _transforms.GetRotation(_entityTransformLookup[entity.index])
		: nullptr;
}

JVec3* JScene::GetTransformScale(JTransformHandle handle)
{
	return _transforms.GetScale(handle);
}

const JVec3* JScene::GetTransformScale(JTransformHandle handle) const
{
	return _transforms.GetScale(handle);
}

JVec3* JScene::GetTransformScale(JEntityHandle entity)
{
	return _entities.IsValid(entity) && entity.index < _entityTransformLookup.size()
		? _transforms.GetScale(_entityTransformLookup[entity.index])
		: nullptr;
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

	return _entityTransformLookup[entity.index];
}

JScene::CameraData* JScene::GetCamera(JCameraHandle handle)
{
	return _cameras.Get(handle);
}

const JScene::CameraData* JScene::GetCamera(JCameraHandle handle) const
{
	return _cameras.Get(handle);
}

JScene::LightData* JScene::GetLight(JLightHandle handle)
{
	return _lights.Get(handle);
}

const JScene::LightData* JScene::GetLight(JLightHandle handle) const
{
	return _lights.Get(handle);
}

JScene::RenderObjectComponentData* JScene::GetRenderObjectComponent(JRenderObjectComponentHandle handle)
{
	return _renderObjectComponents.Get(handle);
}

const JScene::RenderObjectComponentData* JScene::GetRenderObjectComponent(JRenderObjectComponentHandle handle) const
{
	return _renderObjectComponents.Get(handle);
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

std::vector<JSceneRenderObjectEvent> JScene::ConsumeRenderObjectEvents()
{
	std::vector<JSceneRenderObjectEvent> events = std::move(_renderObjectEvents);
	_renderObjectEvents.clear();
	return events;
}

J_ENGINE_END
