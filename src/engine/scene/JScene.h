#pragma once
#ifndef __J_SCENE_H__
#define __J_SCENE_H__

#include "engine/precompile.h"
#include "engine/core/JPool.h"
#include "engine/scene/JSceneHandle.h"
#include "engine/scene/JComponentLayoutDefinition.h"
#include "engine/scene/JCameraPool.h"
#include "engine/scene/JLightPool.h"
#include "engine/scene/JDrawComponentPool.h"
#include "engine/scene/JTransformPool.h"
/*#include "engine/asset/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }

J_ENGINE_BEGIN

enum class JSceneComponentMask : uint32
{
	None = 0,
	Transform = 1 << 0,
	Camera = 1 << 1,
	Light = 1 << 2,
	DrawComponent = 1 << 3,
};

struct JEntityMetadata
{
	uint64 stableIDHash = 0;
	uint32 componentMask = 0;
	std::string stableID;
	std::string name;
	std::vector<std::string> tags;
};

class JScene
{
public:
	struct EntityData
	{
		bool active = true;
	};

	using TransformData = JTransformComponents;
	using CameraData = JCameraComponents;
	using LightData = JLightComponents;
	using DrawComponentData = JDrawComponent;

	using EntityPool = JPool<JEntityHandle, EntityData>;
	using TransformSlot = JTransformPool::SlotType;
	using CameraPool = JCameraPool;
	using LightPool = JLightPool;
	using DrawComponentPool = JDrawComponentPool;

	using EntitySlot = EntityPool::SlotType;
	using CameraSlot = CameraPool::SlotType;
	using LightSlot = LightPool::SlotType;
	using DrawComponentSlot = DrawComponentPool::SlotType;

	JScene() = default;

	JEntityHandle CreateEntity(const std::string& stableID = "", const std::string& name = "", const std::vector<std::string>& tags = {});
	JTransformHandle AddTransform(JEntityHandle entity, const TransformData& data = {});
	JCameraHandle AddCamera(JEntityHandle entity, JTransformHandle transform, float aspectRatio = 1.0f, float nearP = 0.5f, float farP = 1000.0f);
	JLightHandle AddLight(JEntityHandle entity, const LightData& data = {});
	JDrawComponentHandle AddDrawComponent(JEntityHandle entity, uint32 materialID, const JMesh* mesh, bool transparent = false, uint32 subMeshIndex = 0);

	bool SetEntityMetadata(JEntityHandle entity, const std::string& stableID, const std::string& name, const std::vector<std::string>& tags = {});
	void SetEntityName(JEntityHandle entity, const std::string& name);
	const JEntityMetadata* GetEntityMetadata(JEntityHandle entity) const;
	JEntityMetadata* GetEntityMetadata(JEntityHandle entity);
	JEntityHandle FindEntityByStableID(const std::string& stableID) const;

	void SetPrimaryCamera(JCameraHandle camera) { _primaryCamera = camera; }
	JCameraHandle GetPrimaryCamera() const { return _primaryCamera; }

	EntityData* GetEntity(JEntityHandle handle);
	const EntityData* GetEntity(JEntityHandle handle) const;
	TransformData GetTransform(JTransformHandle handle) const;
	TransformData GetTransform(JEntityHandle entity) const;
	void SetTransform(JTransformHandle handle, const TransformData& data);
	void SetTransform(JEntityHandle entity, const TransformData& data);
	void SetTransformTranslation(JTransformHandle handle, const JVec3& value);
	void SetTransformTranslation(JEntityHandle entity, const JVec3& value);
	void SetTransformRotation(JTransformHandle handle, const JVec3& value);
	void SetTransformRotation(JEntityHandle entity, const JVec3& value);
	void SetTransformScale(JTransformHandle handle, const JVec3& value);
	void SetTransformScale(JEntityHandle entity, const JVec3& value);
	std::vector<uint32> ConsumeDirtyTransformIndices();
	bool HasDirtyTransforms() const;
	JVec3* GetTransformTranslation(JTransformHandle handle);
	const JVec3* GetTransformTranslation(JTransformHandle handle) const;
	JVec3* GetTransformTranslation(JEntityHandle entity);
	const JVec3* GetTransformTranslation(JEntityHandle entity) const;
	JVec3* GetTransformRotation(JTransformHandle handle);
	const JVec3* GetTransformRotation(JTransformHandle handle) const;
	JVec3* GetTransformRotation(JEntityHandle entity);
	const JVec3* GetTransformRotation(JEntityHandle entity) const;
	JVec3* GetTransformScale(JTransformHandle handle);
	const JVec3* GetTransformScale(JTransformHandle handle) const;
	JVec3* GetTransformScale(JEntityHandle entity);
	const JVec3* GetTransformScale(JEntityHandle entity) const;
	JTransformHandle GetTransformHandle(JEntityHandle entity) const;
	CameraData* GetCamera(JCameraHandle handle);
	const CameraData* GetCamera(JCameraHandle handle) const;
	LightData* GetLight(JLightHandle handle);
	const LightData* GetLight(JLightHandle handle) const;
	DrawComponentData* GetDrawComponent(JDrawComponentHandle handle);
	const DrawComponentData* GetDrawComponent(JDrawComponentHandle handle) const;

	const std::vector<EntitySlot>& GetEntitySlots() const { return _entities.GetSlots(); }
	const std::vector<TransformSlot>& GetTransformSlots() const { return _transforms.GetSlots(); }
	const std::vector<CameraSlot>& GetCameraSlots() const { return _cameras.GetSlots(); }
	const std::vector<LightSlot>& GetLightSlots() const { return _lights.GetSlots(); }
	const std::vector<DrawComponentSlot>& GetDrawComponentSlots() const { return _drawComponents.GetSlots(); }

private:
	std::string generateStableID();
	void addEntityComponentMask(JEntityHandle entity, JSceneComponentMask component);

	EntityPool _entities;
	JTransformPool _transforms;
	CameraPool _cameras;
	LightPool _lights;
	DrawComponentPool _drawComponents;
	std::vector<JEntityMetadata> _entityMetadata;
	std::vector<JTransformHandle> _entityTransformLookup;
	std::unordered_map<uint64, JEntityHandle> _stableIDLookup;
	JCameraHandle _primaryCamera = {};
	uint32 _nextStableID = 1;
};

J_ENGINE_END

#endif
