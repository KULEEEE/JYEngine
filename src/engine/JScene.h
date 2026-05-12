#pragma once
#ifndef __J_SCENE_H__
#define __J_SCENE_H__

#include "engine/precompile.h"

/*#include "engine/asset/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }

J_ENGINE_BEGIN

struct JEntityHandle
{
	uint32 index = static_cast<uint32>(-1);
	uint32 generation = 0;

	bool IsValid() const { return index != static_cast<uint32>(-1); }
};

struct JTransformHandle
{
	uint32 index = static_cast<uint32>(-1);
	uint32 generation = 0;

	bool IsValid() const { return index != static_cast<uint32>(-1); }
};

struct JCameraHandle
{
	uint32 index = static_cast<uint32>(-1);
	uint32 generation = 0;

	bool IsValid() const { return index != static_cast<uint32>(-1); }
};

struct JLightHandle
{
	uint32 index = static_cast<uint32>(-1);
	uint32 generation = 0;

	bool IsValid() const { return index != static_cast<uint32>(-1); }
};

struct JRenderObjectHandle
{
	uint32 index = static_cast<uint32>(-1);
	uint32 generation = 0;

	bool IsValid() const { return index != static_cast<uint32>(-1); }
};

class JScene
{
public:
	struct EntityData
	{
		bool active = true;
	};

	struct TransformData
	{
		JVec3 position = { 0.0f, 0.0f, 0.0f };
		float yaw = 0.0f;
		float pitch = 0.0f;
	};

	struct CameraData
	{
		JEntityHandle entity = {};
		JTransformHandle transform = {};
		float moveSpeed = 0.1f;
		float rotateSpeed = 1.0f;
		float aspectRatio = 1.0f;
		bool active = true;
	};

	struct LightData
	{
		JEntityHandle entity = {};
		JTransformHandle transform = {};
		JVec3 color = { 1.0f, 1.0f, 1.0f };
		float intensity = 1.0f;
		bool active = true;
	};

	struct RenderObjectData
	{
		JEntityHandle entity = {};
		JTransformHandle transform = {};
		uint32 materialID = 0;
		const JMesh* mesh = nullptr;
		bool visible = true;
		bool transparent = false;
		bool active = true;
	};

	template <typename T>
	struct PoolSlot
	{
		uint32 generation = 1;
		bool active = false;
		T data = {};
	};

	using EntitySlot = PoolSlot<EntityData>;
	using TransformSlot = PoolSlot<TransformData>;
	using CameraSlot = PoolSlot<CameraData>;
	using LightSlot = PoolSlot<LightData>;
	using RenderObjectSlot = PoolSlot<RenderObjectData>;

	JScene() = default;

	JEntityHandle CreateEntity();
	JTransformHandle AddTransform(JEntityHandle entity, const TransformData& data = {});
	JCameraHandle AddCamera(JEntityHandle entity, JTransformHandle transform, float aspectRatio = 1.0f);
	JLightHandle AddLight(JEntityHandle entity, JTransformHandle transform, const LightData& data = {});
	JRenderObjectHandle AddRenderObject(JEntityHandle entity, JTransformHandle transform, uint32 materialID, const JMesh* mesh, bool transparent = false);

	void SetPrimaryCamera(JCameraHandle camera) { _primaryCamera = camera; }
	JCameraHandle GetPrimaryCamera() const { return _primaryCamera; }

	EntityData* GetEntity(JEntityHandle handle);
	const EntityData* GetEntity(JEntityHandle handle) const;
	TransformData* GetTransform(JTransformHandle handle);
	const TransformData* GetTransform(JTransformHandle handle) const;
	CameraData* GetCamera(JCameraHandle handle);
	const CameraData* GetCamera(JCameraHandle handle) const;
	LightData* GetLight(JLightHandle handle);
	const LightData* GetLight(JLightHandle handle) const;
	RenderObjectData* GetRenderObject(JRenderObjectHandle handle);
	const RenderObjectData* GetRenderObject(JRenderObjectHandle handle) const;

	const std::vector<EntitySlot>& GetEntitySlots() const { return _entities; }
	const std::vector<TransformSlot>& GetTransformSlots() const { return _transforms; }
	const std::vector<CameraSlot>& GetCameraSlots() const { return _cameras; }
	const std::vector<LightSlot>& GetLightSlots() const { return _lights; }
	const std::vector<RenderObjectSlot>& GetRenderObjectSlots() const { return _renderObjects; }

	void RotateCamera(JCameraHandle camera, float yawDelta, float pitchDelta);
	void MoveCameraLocal(JCameraHandle camera, float forward, float right, float up, float deltaTime);
	XMMATRIX GetCameraViewMatrix(JCameraHandle camera) const;
	XMMATRIX GetCameraProjectionMatrix(JCameraHandle camera) const;

private:
	static uint64 MakeHandleKey(uint32 index, uint32 generation)
	{
		return (static_cast<uint64>(generation) << 32) | index;
	}

	bool IsValidEntity(JEntityHandle handle) const;
	bool IsValidTransform(JTransformHandle handle) const;
	bool IsValidCamera(JCameraHandle handle) const;
	bool IsValidLight(JLightHandle handle) const;
	bool IsValidRenderObject(JRenderObjectHandle handle) const;

	std::vector<EntitySlot> _entities;
	std::vector<TransformSlot> _transforms;
	std::vector<CameraSlot> _cameras;
	std::vector<LightSlot> _lights;
	std::vector<RenderObjectSlot> _renderObjects;
	JCameraHandle _primaryCamera = {};
};

J_ENGINE_END

#endif
