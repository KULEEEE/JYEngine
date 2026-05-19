#pragma once
#ifndef __J_RENDER_OBJECT_POOL_H__
#define __J_RENDER_OBJECT_POOL_H__

#include "engine/precompile.h"
#include "engine/core/JPool.h"
#include "engine/scene/JComponentLayoutDefinition.h"

J_ENGINE_BEGIN

class JRenderObjectPool
{
public:
	using SlotType = JPool<JRenderObjectHandle, JDrawComponent>::SlotType;

	JRenderObjectHandle Add(JEntityHandle entity, const JDrawComponent& data = {})
	{
		JDrawComponent resolved = data;
		resolved.entity = entity;
		return _pool.Add(resolved);
	}

	bool IsValid(JRenderObjectHandle handle) const
	{
		return _pool.IsValid(handle);
	}

	JDrawComponent* Get(JRenderObjectHandle handle)
	{
		return _pool.Get(handle);
	}

	const JDrawComponent* Get(JRenderObjectHandle handle) const
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
	JPool<JRenderObjectHandle, JDrawComponent> _pool;
};

J_ENGINE_END

#endif
