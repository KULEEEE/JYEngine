#pragma once
#ifndef __J_TRANSFORM_POOL_H__
#define __J_TRANSFORM_POOL_H__

#include "engine/precompile.h"
#include "engine/scene/JSceneHandle.h"
#include "engine/scene/JComponentLayoutDefinition.h"

J_ENGINE_BEGIN

class JTransformPool
{
public:
	struct SlotType
	{
		uint32 generation = 1;
		bool active = false;
		JEntityHandle entity = {};
	};

	JTransformHandle Add(JEntityHandle entity, const JTransformComponents& data = {});
	bool IsValid(JTransformHandle handle) const;

	JTransformComponents Get(JTransformHandle handle) const;
	JVec3* GetTranslation(JTransformHandle handle);
	const JVec3* GetTranslation(JTransformHandle handle) const;
	JVec3* GetRotation(JTransformHandle handle);
	const JVec3* GetRotation(JTransformHandle handle) const;
	JVec3* GetScale(JTransformHandle handle);
	const JVec3* GetScale(JTransformHandle handle) const;

	void Set(JTransformHandle handle, const JTransformComponents& data);
	void SetTranslation(JTransformHandle handle, const JVec3& value);
	void SetRotation(JTransformHandle handle, const JVec3& value);
	void SetScale(JTransformHandle handle, const JVec3& value);

	const std::vector<SlotType>& GetSlots() const { return _slots; }

private:
	uint32 findIndex(JTransformHandle handle) const;

	std::vector<SlotType> _slots;
	std::vector<JVec3> _translations;
	std::vector<JVec3> _rotations;
	std::vector<JVec3> _scales;
};

J_ENGINE_END

#endif
