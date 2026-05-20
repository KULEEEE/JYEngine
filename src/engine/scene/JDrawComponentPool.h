#pragma once
#ifndef __J_DRAW_COMPONENT_POOL_H__
#define __J_DRAW_COMPONENT_POOL_H__

#include "engine/precompile.h"
#include "engine/core/JPool.h"
#include "engine/scene/JComponentLayoutDefinition.h"
#include "engine/scene/JSceneHandle.h"

J_ENGINE_BEGIN

class JDrawComponentPool
{
public:
	using SlotType = JPool<JDrawComponentHandle, JDrawComponent>::SlotType;

	JDrawComponentHandle Add(JEntityHandle entity, const JDrawComponent& data = {})
	{
		JDrawComponent resolved = data;
		resolved.entity = entity;
		return _pool.Add(resolved);
	}

	bool IsValid(JDrawComponentHandle handle) const
	{
		return _pool.IsValid(handle);
	}

	JDrawComponent* Get(JDrawComponentHandle handle)
	{
		return _pool.Get(handle);
	}

	const JDrawComponent* Get(JDrawComponentHandle handle) const
	{
		return _pool.Get(handle);
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
	JPool<JDrawComponentHandle, JDrawComponent> _pool;
};

J_ENGINE_END

#endif
