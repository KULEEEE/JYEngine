#pragma once
#ifndef __J_POOL_H__
#define __J_POOL_H__

#include "engine/precompile.h"

J_ENGINE_BEGIN

template <typename TData>
struct JPoolSlot
{
	uint32 generation = 1;
	bool active = false;
	TData data = {};
};

template <typename THandle, typename TData>
class JPool
{
public:
	using HandleType = THandle;
	using DataType = TData;
	using SlotType = JPoolSlot<TData>;

	THandle Add(const TData& data = {})
	{
		SlotType slot;
		slot.active = true;
		slot.data = data;
		_slots.push_back(slot);
		return { static_cast<uint32>(_slots.size() - 1), slot.generation };
	}

	TData* Get(THandle handle)
	{
		return IsValid(handle) ? &_slots[handle.index].data : nullptr;
	}

	const TData* Get(THandle handle) const
	{
		return IsValid(handle) ? &_slots[handle.index].data : nullptr;
	}

	bool IsValid(THandle handle) const
	{
		return handle.IsValid()
			&& handle.index < _slots.size()
			&& _slots[handle.index].active
			&& _slots[handle.index].generation == handle.generation;
	}

	const std::vector<SlotType>& GetSlots() const
	{
		return _slots;
	}

	std::vector<SlotType>& GetSlots()
	{
		return _slots;
	}

private:
	std::vector<SlotType> _slots;
};

J_ENGINE_END

#endif
