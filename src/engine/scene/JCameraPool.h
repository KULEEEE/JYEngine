#pragma once
#ifndef __J_CAMERA_POOL_H__
#define __J_CAMERA_POOL_H__

#include "engine/precompile.h"
#include "engine/core/JPool.h"
#include "engine/scene/JComponentLayoutDefinition.h"

J_ENGINE_BEGIN

class JCameraPool
{
public:
	using SlotType = JEntityComponentPool<JCameraHandle, JCameraComponents>::SlotType;

	JCameraHandle Add(JEntityHandle entity, const JCameraComponents& data = {})
	{
		JCameraComponents resolved = data;
		resolved.entity = entity;
		return _pool.Add(entity, resolved);
	}

	bool IsValid(JCameraHandle handle) const
	{
		return _pool.IsValid(handle);
	}

	JCameraComponents* Get(JCameraHandle handle)
	{
		return _pool.Get(handle);
	}

	const JCameraComponents* Get(JCameraHandle handle) const
	{
		return _pool.Get(handle);
	}

	bool Remove(JCameraHandle handle)
	{
		return _pool.Remove(handle);
	}

	const std::vector<SlotType>& GetSlots() const
	{
		return _pool.GetSlots();
	}

	std::vector<SlotType>& GetSlots()
	{
		return _pool.GetSlots();
	}

private:
	JEntityComponentPool<JCameraHandle, JCameraComponents> _pool;
};

J_ENGINE_END

#endif
