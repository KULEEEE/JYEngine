#pragma once
#ifndef __J_LIGHT_POOL_H__
#define __J_LIGHT_POOL_H__

#include "engine/precompile.h"
#include "engine/core/JPool.h"
#include "engine/scene/JComponentLayoutDefinition.h"

J_ENGINE_BEGIN

class JLightPool
{
public:
	using SlotType = JEntityComponentPool<JLightHandle, JLightComponents>::SlotType;

	JLightHandle Add(JEntityHandle entity, const JLightComponents& data = {})
	{
		JLightComponents resolved = data;
		resolved.entity = entity;
		return _pool.Add(entity, resolved);
	}

	bool IsValid(JLightHandle handle) const
	{
		return _pool.IsValid(handle);
	}

	JLightComponents* Get(JLightHandle handle)
	{
		return _pool.Get(handle);
	}

	const JLightComponents* Get(JLightHandle handle) const
	{
		return _pool.Get(handle);
	}

	bool Remove(JLightHandle handle)
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
	JEntityComponentPool<JLightHandle, JLightComponents> _pool;
};

J_ENGINE_END

#endif
