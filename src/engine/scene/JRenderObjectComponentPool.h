#pragma once
#ifndef __J_RENDER_OBJECT_COMPONENT_POOL_H__
#define __J_RENDER_OBJECT_COMPONENT_POOL_H__

#include "engine/precompile.h"
#include "engine/core/JPool.h"
#include "engine/scene/JComponentLayoutDefinition.h"
#include "engine/scene/JSceneHandle.h"

J_ENGINE_BEGIN

class JRenderObjectComponentPool
{
public:
	using SlotType = JPool<JRenderObjectComponentHandle, JRenderObjectComponent>::SlotType;

	JRenderObjectComponentHandle Add(JEntityHandle entity, const JRenderObjectComponent& data = {})
	{
		JRenderObjectComponent resolved = data;
		resolved.entity = entity;
		return _pool.Add(resolved);
	}

	bool IsValid(JRenderObjectComponentHandle handle) const
	{
		return _pool.IsValid(handle);
	}

	JRenderObjectComponent* Get(JRenderObjectComponentHandle handle)
	{
		return _pool.Get(handle);
	}

	const JRenderObjectComponent* Get(JRenderObjectComponentHandle handle) const
	{
		return _pool.Get(handle);
	}

	bool Remove(JRenderObjectComponentHandle handle)
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
	JPool<JRenderObjectComponentHandle, JRenderObjectComponent> _pool;
};

J_ENGINE_END

#endif
