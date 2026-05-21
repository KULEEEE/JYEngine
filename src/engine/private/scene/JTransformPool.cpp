#include "engine/scene/JTransformPool.h"

J_ENGINE_BEGIN

uint32 JTransformPool::findIndex(JTransformHandle handle) const
{
	return IsValid(handle) ? handle.index : static_cast<uint32>(-1);
}

JTransformHandle JTransformPool::Add(JEntityHandle entity, const JTransformComponents& data)
{
	if (!entity.IsValid())
	{
		return {};
	}

	if (entity.index >= _slots.size())
	{
		constexpr uint32 CHUNK_SIZE = 256;
		const uint32 newSize = ((entity.index + 1 + CHUNK_SIZE - 1) / CHUNK_SIZE) * CHUNK_SIZE;
		_slots.resize(newSize);
		_translations.resize(newSize);
		_rotations.resize(newSize);
		_scales.resize(newSize);
		_dirtyFlags.resize(newSize);
	}

	SlotType& slot = _slots[entity.index];
	if (slot.active && slot.generation == entity.generation)
	{
		return {};
	}

	slot.active = true;
	slot.generation = entity.generation;
	slot.entity = entity;
	_translations[entity.index] = data.translation;
	_rotations[entity.index] = data.rotation;
	_scales[entity.index] = data.scale;
	_dirtyFlags[entity.index] = true;
	_dirtyIndices.push_back(entity.index);
	return { entity.index, entity.generation };
}

bool JTransformPool::IsValid(JTransformHandle handle) const
{
	return handle.IsValid()
		&& handle.index < _slots.size()
		&& _slots[handle.index].active
		&& _slots[handle.index].generation == handle.generation;
}

bool JTransformPool::Remove(JTransformHandle handle)
{
	if (!IsValid(handle))
	{
		return false;
	}

	const uint32 index = handle.index;
	_slots[index].active = false;
	_slots[index].entity = {};
	_translations[index] = {};
	_rotations[index] = {};
	_scales[index] = {};
	_dirtyFlags[index] = false;
	return true;
}

JTransformComponents JTransformPool::Get(JTransformHandle handle) const
{
	if (!IsValid(handle))
	{
		return {};
	}

	const uint32 index = handle.index;
	JTransformComponents data{};
	data.translation = _translations[index];
	data.rotation = _rotations[index];
	data.scale = _scales[index];
	return data;
}

JVec3* JTransformPool::GetTranslation(JTransformHandle handle)
{
	return IsValid(handle) ? &_translations[handle.index] : nullptr;
}

const JVec3* JTransformPool::GetTranslation(JTransformHandle handle) const
{
	return IsValid(handle) ? &_translations[handle.index] : nullptr;
}

JVec3* JTransformPool::GetRotation(JTransformHandle handle)
{
	return IsValid(handle) ? &_rotations[handle.index] : nullptr;
}

const JVec3* JTransformPool::GetRotation(JTransformHandle handle) const
{
	return IsValid(handle) ? &_rotations[handle.index] : nullptr;
}

JVec3* JTransformPool::GetScale(JTransformHandle handle)
{
	return IsValid(handle) ? &_scales[handle.index] : nullptr;
}

const JVec3* JTransformPool::GetScale(JTransformHandle handle) const
{
	return IsValid(handle) ? &_scales[handle.index] : nullptr;
}

void JTransformPool::Set(JTransformHandle handle, const JTransformComponents& data)
{
	SetTranslation(handle, data.translation);
	SetRotation(handle, data.rotation);
	SetScale(handle, data.scale);
}

void JTransformPool::SetTranslation(JTransformHandle handle, const JVec3& value)
{
	if (JVec3* translation = GetTranslation(handle))
	{
		*translation = value;
		markDirty(handle.index);
	}
}

void JTransformPool::SetRotation(JTransformHandle handle, const JVec3& value)
{
	if (JVec3* rotation = GetRotation(handle))
	{
		*rotation = value;
		markDirty(handle.index);
	}
}

void JTransformPool::SetScale(JTransformHandle handle, const JVec3& value)
{
	if (JVec3* scale = GetScale(handle))
	{
		*scale = value;
		markDirty(handle.index);
	}
}

std::vector<uint32> JTransformPool::ConsumeDirtyIndices()
{
	std::vector<uint32> dirtyIndices = std::move(_dirtyIndices);
	for (uint32 index : dirtyIndices)
	{
		if (index < _dirtyFlags.size())
		{
			_dirtyFlags[index] = false;
		}
	}
	return dirtyIndices;
}

void JTransformPool::markDirty(uint32 index)
{
	if (index >= _dirtyFlags.size())
	{
		return;
	}

	if (_dirtyFlags[index])
	{
		return;
	}

	_dirtyFlags[index] = true;
	_dirtyIndices.push_back(index);
}

J_ENGINE_END
