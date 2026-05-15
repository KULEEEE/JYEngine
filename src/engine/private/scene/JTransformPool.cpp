#include "engine/scene/JTransformPool.h"

J_ENGINE_BEGIN

uint32 JTransformPool::findIndex(JTransformHandle handle) const
{
	return IsValid(handle) ? handle.index : static_cast<uint32>(-1);
}

JTransformHandle JTransformPool::Add(JEntityHandle entity, const JTransformComponents& data)
{
	SlotType slot;
	slot.active = true;
	slot.entity = entity;
	_slots.push_back(slot);
	_translations.push_back(data.translation);
	_rotations.push_back(data.rotation);
	_scales.push_back(data.scale);
	return { static_cast<uint32>(_slots.size() - 1), slot.generation };
}

bool JTransformPool::IsValid(JTransformHandle handle) const
{
	return handle.IsValid()
		&& handle.index < _slots.size()
		&& _slots[handle.index].active
		&& _slots[handle.index].generation == handle.generation;
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
	}
}

void JTransformPool::SetRotation(JTransformHandle handle, const JVec3& value)
{
	if (JVec3* rotation = GetRotation(handle))
	{
		*rotation = value;
	}
}

void JTransformPool::SetScale(JTransformHandle handle, const JVec3& value)
{
	if (JVec3* scale = GetScale(handle))
	{
		*scale = value;
	}
}

J_ENGINE_END
