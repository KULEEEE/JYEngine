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

std::string JScene::GenerateStableID()
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

	const std::string resolvedStableID = stableID.empty() ? GenerateStableID() : stableID;
	SetEntityMetadata(entity, resolvedStableID, name, tags);
	return entity;
}

bool JScene::SetEntityMetadata(JEntityHandle entity, const std::string& stableID, const std::string& name, const std::vector<std::string>& tags)
{
	if (!_entities.IsValid(entity))
	{
		return false;
	}

	const std::string resolvedStableID = stableID.empty() ? GenerateStableID() : stableID;
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

void JScene::AddEntityComponentMask(JEntityHandle entity, JSceneComponentMask component)
{
	JEntityMetadata* metadata = GetEntityMetadata(entity);
	if (metadata != nullptr)
	{
		metadata->componentMask |= static_cast<uint32>(component);
	}
}

JTransformHandle JScene::AddTransform(JEntityHandle entity, const TransformData& data)
{
	if (!_entities.IsValid(entity))
	{
		return {};
	}

	const JTransformHandle transform = _transforms.Add(data);
	AddEntityComponentMask(entity, JSceneComponentMask::Transform);
	return transform;
}

JCameraHandle JScene::AddCamera(JEntityHandle entity, JTransformHandle transform, float aspectRatio)
{
	if (!_entities.IsValid(entity) || !_transforms.IsValid(transform))
	{
		return {};
	}

	CameraData data;
	data.entity = entity;
	data.transform = transform;
	data.aspectRatio = aspectRatio;

	const JCameraHandle handle = _cameras.Add(data);
	if (!_primaryCamera.IsValid())
	{
		_primaryCamera = handle;
	}
	AddEntityComponentMask(entity, JSceneComponentMask::Camera);
	return handle;
}

JLightHandle JScene::AddLight(JEntityHandle entity, JTransformHandle transform, const LightData& data)
{
	if (!_entities.IsValid(entity) || !_transforms.IsValid(transform))
	{
		return {};
	}

	LightData lightData = data;
	lightData.entity = entity;
	lightData.transform = transform;
	const JLightHandle light = _lights.Add(lightData);
	AddEntityComponentMask(entity, JSceneComponentMask::Light);
	return light;
}

JRenderObjectHandle JScene::AddRenderObject(JEntityHandle entity, JTransformHandle transform, uint32 materialID, const JMesh* mesh, bool transparent)
{
	if (!_entities.IsValid(entity) || !_transforms.IsValid(transform))
	{
		return {};
	}

	RenderObjectData data;
	data.entity = entity;
	data.transform = transform;
	data.materialID = materialID;
	data.mesh = mesh;
	data.transparent = transparent;
	const JRenderObjectHandle renderObject = _renderObjects.Add(data);
	AddEntityComponentMask(entity, JSceneComponentMask::RenderObject);
	return renderObject;
}

JScene::EntityData* JScene::GetEntity(JEntityHandle handle)
{
	return _entities.Get(handle);
}

const JScene::EntityData* JScene::GetEntity(JEntityHandle handle) const
{
	return _entities.Get(handle);
}

JScene::TransformData* JScene::GetTransform(JTransformHandle handle)
{
	return _transforms.Get(handle);
}

const JScene::TransformData* JScene::GetTransform(JTransformHandle handle) const
{
	return _transforms.Get(handle);
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

JScene::RenderObjectData* JScene::GetRenderObject(JRenderObjectHandle handle)
{
	return _renderObjects.Get(handle);
}

const JScene::RenderObjectData* JScene::GetRenderObject(JRenderObjectHandle handle) const
{
	return _renderObjects.Get(handle);
}

J_ENGINE_END
