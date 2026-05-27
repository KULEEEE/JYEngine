#pragma once
#ifndef __J_SCENE_H__
#define __J_SCENE_H__

#include "engine/precompile.h"
#include "engine/core/JPool.h"
#include "engine/scene/JSceneHandle.h"
#include "engine/scene/JComponentLayoutDefinition.h"
#include "engine/scene/JCameraPool.h"
#include "engine/scene/JLightPool.h"
#include "engine/scene/JRenderObjectComponentPool.h"
#include "engine/scene/JTransformPool.h"
/*#include "engine/asset/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }
/*#include "engine/asset/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }

J_ENGINE_BEGIN

enum class JSceneComponentMask : uint32
{
	None = 0,
	Transform = 1 << 0,
	Camera = 1 << 1,
	Light = 1 << 2,
	RenderObject = 1 << 3,
};

struct JEntityMetadata
{
	uint64 stableIDHash = 0;
	uint32 componentMask = 0;
	std::string stableID;
	std::string name;
	std::vector<std::string> tags;
};

enum class JSceneRenderObjectEventType : uint8
{
	Added,
	Modified,
	Removed
};

struct JSceneRenderObjectEvent
{
	JSceneRenderObjectEventType type = JSceneRenderObjectEventType::Modified;
	JRenderObjectComponentHandle handle = {};
	JEntityHandle entity = {};
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
	using RenderObjectComponentData = JRenderObjectComponent;

	using EntityPool = JEntityPool<EntityData>;
	using TransformSlot = JTransformPool::SlotType;
	using CameraPool = JCameraPool;
	using LightPool = JLightPool;
	using RenderObjectComponentPool = JRenderObjectComponentPool;

	using EntitySlot = EntityPool::SlotType;
	using CameraSlot = CameraPool::SlotType;
	using LightSlot = LightPool::SlotType;
	using RenderObjectComponentSlot = RenderObjectComponentPool::SlotType;

	JScene() = default;

	struct MaterialSlot
	{
		uint32 generation = 1;
		bool active = false;
		std::shared_ptr<JMaterial> material;
	};

	JEntityHandle CreateEntity(const std::string& stableID = "", const std::string& name = "", const std::vector<std::string>& tags = {});
	bool RemoveEntity(JEntityHandle entity);
	JMaterialHandle AddMaterial(std::shared_ptr<JMaterial> material);
	bool RemoveMaterial(JMaterialHandle material);
	JTransformHandle AddTransform(JEntityHandle entity, const TransformData& data = {});
	JCameraHandle AddCamera(JEntityHandle entity, JTransformHandle transform, float aspectRatio = 1.0f, float nearP = 0.5f, float farP = 1000.0f);
	JCameraHandle AddCamera(JEntityHandle entity, float aspectRatio = 1.0f, float nearP = 0.5f, float farP = 1000.0f);
	JLightHandle AddLight(JEntityHandle entity, const LightData& data = {});
	JRenderObjectComponentHandle AddRenderObjectComponent(JEntityHandle entity, JMaterialHandle material, const JMesh* mesh, bool transparent = false);
	bool RemoveTransform(JTransformHandle transform);
	bool RemoveTransform(JEntityHandle entity);
	bool RemoveCamera(JCameraHandle camera);
	bool RemoveCamera(JEntityHandle entity);
	bool RemoveLight(JLightHandle light);
	bool RemoveLight(JEntityHandle entity);
	bool RemoveRenderObjectComponent(JRenderObjectComponentHandle handle);
	bool RemoveRenderObjectComponent(JEntityHandle entity);

	bool SetEntityMetadata(JEntityHandle entity, const std::string& stableID, const std::string& name, const std::vector<std::string>& tags = {});
	void SetEntityName(JEntityHandle entity, const std::string& name);
	const JEntityMetadata* GetEntityMetadata(JEntityHandle entity) const;
	JEntityMetadata* GetEntityMetadata(JEntityHandle entity);
	JEntityHandle FindEntityByStableID(const std::string& stableID) const;

	void SetPrimaryCamera(JCameraHandle camera) { _primaryCamera = camera; }
	void SetPrimaryCamera(JEntityHandle entity) { _primaryCamera = GetCameraHandle(entity); }
	JCameraHandle GetPrimaryCamera() const { return _primaryCamera; }

	EntityData* GetEntity(JEntityHandle handle);
	const EntityData* GetEntity(JEntityHandle handle) const;
	JMaterial* GetMaterial(JMaterialHandle handle);
	const JMaterial* GetMaterial(JMaterialHandle handle) const;
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
	const JVec3* GetTransformTranslation(JTransformHandle handle) const;
	const JVec3* GetTransformTranslation(JEntityHandle entity) const;
	const JVec3* GetTransformRotation(JTransformHandle handle) const;
	const JVec3* GetTransformRotation(JEntityHandle entity) const;
	const JVec3* GetTransformScale(JTransformHandle handle) const;
	const JVec3* GetTransformScale(JEntityHandle entity) const;
	JTransformHandle GetTransformHandle(JEntityHandle entity) const;
	JCameraHandle GetCameraHandle(JEntityHandle entity) const;
	JLightHandle GetLightHandle(JEntityHandle entity) const;
	JRenderObjectComponentHandle GetRenderObjectComponentHandle(JEntityHandle entity) const;
	CameraData* GetCamera(JCameraHandle handle);
	const CameraData* GetCamera(JCameraHandle handle) const;
	CameraData* GetCamera(JEntityHandle entity);
	const CameraData* GetCamera(JEntityHandle entity) const;
	LightData* GetLight(JLightHandle handle);
	const LightData* GetLight(JLightHandle handle) const;
	LightData* GetLight(JEntityHandle entity);
	const LightData* GetLight(JEntityHandle entity) const;
	RenderObjectComponentData* GetRenderObjectComponent(JRenderObjectComponentHandle handle);
	const RenderObjectComponentData* GetRenderObjectComponent(JRenderObjectComponentHandle handle) const;
	RenderObjectComponentData* GetRenderObjectComponent(JEntityHandle entity);
	const RenderObjectComponentData* GetRenderObjectComponent(JEntityHandle entity) const;
	void SetCameraData(JCameraHandle camera, const CameraData& data);
	void SetCameraAspectRatio(JCameraHandle camera, float aspectRatio);
	void SetLightData(JLightHandle light, const LightData& data);
	void SetRenderObjectComponentData(JRenderObjectComponentHandle handle, const RenderObjectComponentData& data);
	void SetRenderObjectVisible(JRenderObjectComponentHandle handle, bool visible);
	void SetRenderObjectVisible(JEntityHandle entity, bool visible);
	void MarkCameraDirty(JCameraHandle camera);
	void MarkCameraDirty(JEntityHandle entity);
	std::vector<JCameraHandle> ConsumeDirtyCameras();
	void MarkLightDirty(JLightHandle light);
	void MarkLightDirty(JEntityHandle entity);
	std::vector<JLightHandle> ConsumeDirtyLights();
	void MarkRenderObjectComponentModified(JRenderObjectComponentHandle handle);
	void MarkRenderObjectComponentModified(JEntityHandle entity);
	std::vector<JSceneRenderObjectEvent> ConsumeRenderObjectEvents();

	const std::vector<EntitySlot>& GetEntitySlots() const { return _entities.GetSlots(); }
	const std::vector<TransformSlot>& GetTransformSlots() const { return _transforms.GetSlots(); }
	const std::vector<CameraSlot>& GetCameraSlots() const { return _cameras.GetSlots(); }
	const std::vector<LightSlot>& GetLightSlots() const { return _lights.GetSlots(); }
	const std::vector<RenderObjectComponentSlot>& GetRenderObjectComponentSlots() const { return _renderObjectComponents.GetSlots(); }
	const std::vector<uint32>& GetActiveRenderObjectComponentIndices() const { return _renderObjectComponents.GetActiveEntityIndices(); }
	const std::vector<MaterialSlot>& GetMaterialSlots() const { return _materials; }
	const std::vector<uint32>& GetActiveMaterialIndices() const { return _activeMaterialIndices; }

private:
	std::string generateStableID();
	void addEntityComponentMask(JEntityHandle entity, JSceneComponentMask component);
	void removeEntityComponentMask(JEntityHandle entity, JSceneComponentMask component);
	void pushRenderObjectEvent(JSceneRenderObjectEventType type, JRenderObjectComponentHandle handle, JEntityHandle entity);
	void markCameraDirtyForEntity(JEntityHandle entity);
	void markLightDirtyForEntity(JEntityHandle entity);

	EntityPool _entities;
	JTransformPool _transforms;
	CameraPool _cameras;
	LightPool _lights;
	RenderObjectComponentPool _renderObjectComponents;
	std::vector<MaterialSlot> _materials;
	std::vector<uint32> _freeMaterialIndices;
	std::vector<uint32> _activeMaterialIndices;
	std::vector<JEntityMetadata> _entityMetadata;
	std::vector<JTransformHandle> _entityTransformLookup;
	std::vector<JSceneRenderObjectEvent> _renderObjectEvents;
	std::vector<JCameraHandle> _dirtyCameras;
	std::vector<JLightHandle> _dirtyLights;
	std::unordered_map<uint64, JEntityHandle> _stableIDLookup;
	JCameraHandle _primaryCamera = {};
	uint32 _nextStableID = 1;
};

J_ENGINE_END

#endif
