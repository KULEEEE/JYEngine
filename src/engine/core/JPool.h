#pragma once
#ifndef __J_POOL_H__
#define __J_POOL_H__

#include "engine/precompile.h"
#include "engine/scene/JSceneHandle.h"

J_ENGINE_BEGIN

template <typename TData>
struct JPoolSlot
{
	uint32 generation = 1;
	bool active = false;
	TData data = {};
};

template <typename TData>
class JEntityPool
{
public:
	using DataType = TData;
	using SlotType = JPoolSlot<TData>;

	JEntityHandle Add(const TData& data = {})
	{
		if (!_freeIndices.empty())
		{
			const auto iter = std::min_element(_freeIndices.begin(), _freeIndices.end());
			const uint32 index = *iter;
			_freeIndices.erase(iter);

			SlotType& slot = _slots[index];
			slot.active = true;
			slot.data = data;
			return { index, slot.generation };
		}

		SlotType slot;
		slot.active = true;
		slot.data = data;
		_slots.push_back(slot);
		return { static_cast<uint32>(_slots.size() - 1), slot.generation };
	}

	bool Remove(JEntityHandle handle)
	{
		if (!IsValid(handle))
		{
			return false;
		}

		SlotType& slot = _slots[handle.index];
		slot.active = false;
		++slot.generation;
		slot.data = {};
		_freeIndices.push_back(handle.index);
		return true;
	}

	TData* Get(JEntityHandle handle)
	{
		return IsValid(handle) ? &_slots[handle.index].data : nullptr;
	}

	const TData* Get(JEntityHandle handle) const
	{
		return IsValid(handle) ? &_slots[handle.index].data : nullptr;
	}

	bool IsValid(JEntityHandle handle) const
	{
		return handle.IsValid()
			&& handle.index < _slots.size()
			&& _slots[handle.index].active
			&& _slots[handle.index].generation == handle.generation;
	}

	const std::vector<SlotType>& GetSlots() const { return _slots; }
	std::vector<SlotType>& GetSlots() { return _slots; }

private:
	std::vector<SlotType> _slots;
	std::vector<uint32> _freeIndices;
};

template <typename THandle, typename TData>
class JEntityComponentPool
{
public:
	using HandleType = THandle;
	using DataType = TData;
	using SlotType = JPoolSlot<TData>;

	THandle Add(JEntityHandle entity, const TData& data = {})
	{
		if (!entity.IsValid())
		{
			return {};
		}

		ensureCapacity(entity.index);

		SlotType& slot = _slots[entity.index];
		if (slot.active && slot.generation == entity.generation)
		{
			return {};
		}

		const bool wasActive = slot.active;
		slot.active = true;
		slot.generation = entity.generation;
		slot.data = data;
		if (!wasActive)
		{
			_activeEntityIndices.push_back(entity.index);
		}
		return { entity.index, entity.generation };
	}

	bool Remove(THandle handle)
	{
		if (!IsValid(handle))
		{
			return false;
		}

		SlotType& slot = _slots[handle.index];
		slot.active = false;
		slot.data = {};
		removeActiveEntityIndex(handle.index);
		return true;
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

	const std::vector<SlotType>& GetSlots() const { return _slots; }
	std::vector<SlotType>& GetSlots() { return _slots; }
	const std::vector<uint32>& GetActiveEntityIndices() const { return _activeEntityIndices; }

private:
	void ensureCapacity(uint32 entityIndex)
	{
		if (entityIndex >= _slots.size())
		{
			_slots.resize(alignCapacity(entityIndex + 1));
		}
	}

	static uint32 alignCapacity(uint32 requiredSize)
	{
		constexpr uint32 CHUNK_SIZE = 256;
		return ((requiredSize + CHUNK_SIZE - 1) / CHUNK_SIZE) * CHUNK_SIZE;
	}

	void removeActiveEntityIndex(uint32 entityIndex)
	{
		for (uint32 i = 0; i < _activeEntityIndices.size(); ++i)
		{
			if (_activeEntityIndices[i] == entityIndex)
			{
				_activeEntityIndices[i] = _activeEntityIndices.back();
				_activeEntityIndices.pop_back();
				return;
			}
		}
	}

	std::vector<SlotType> _slots;
	std::vector<uint32> _activeEntityIndices;
};

J_ENGINE_END

#endif
