#include "engine/scene/JScene.h"

J_ENGINE_BEGIN

namespace
{
	XMVECTOR getForward(float yaw, float pitch)
	{
		const float cosPitch = cosf(pitch);
		return XMVector3Normalize(XMVectorSet(
			sinf(yaw) * cosPitch,
			sinf(pitch),
			cosf(yaw) * cosPitch,
			0.0f));
	}

	XMVECTOR getForwardXZ(float yaw)
	{
		return XMVector3Normalize(XMVectorSet(sinf(yaw), 0.0f, cosf(yaw), 0.0f));
	}

	XMVECTOR getRight(float yaw)
	{
		const XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
		return XMVector3Normalize(XMVector3Cross(worldUp, getForwardXZ(yaw)));
	}

	XMVECTOR getUp(float yaw, float pitch)
	{
		const XMVECTOR forward = getForward(yaw, pitch);
		const XMVECTOR right = getRight(yaw);
		return XMVector3Normalize(XMVector3Cross(forward, right));
	}
}

JEntityHandle JScene::CreateEntity()
{
	EntityData data;
	data.active = true;
	return _entities.Add(data);
}

JTransformHandle JScene::AddTransform(JEntityHandle entity, const TransformData& data)
{
	if (!_entities.IsValid(entity))
	{
		return {};
	}

	return _transforms.Add(data);
}

JCameraHandle JScene::AddCamera(JEntityHandle entity, JTransformHandle transform, float aspectRatio)
{
	if (!_entities.IsValid(entity) || !_transforms.IsValid(transform))
	{
		return {};
	}

	CameraData data;
	data.entity = entity;
	data.transform = transform;
	data.aspectRatio = aspectRatio;

	const JCameraHandle handle = _cameras.Add(data);
	if (!_primaryCamera.IsValid())
	{
		_primaryCamera = handle;
	}
	return handle;
}

JLightHandle JScene::AddLight(JEntityHandle entity, JTransformHandle transform, const LightData& data)
{
	if (!_entities.IsValid(entity) || !_transforms.IsValid(transform))
	{
		return {};
	}

	LightData lightData = data;
	lightData.entity = entity;
	lightData.transform = transform;
	return _lights.Add(lightData);
}

JRenderObjectHandle JScene::AddRenderObject(JEntityHandle entity, JTransformHandle transform, uint32 materialID, const JMesh* mesh, bool transparent)
{
	if (!_entities.IsValid(entity) || !_transforms.IsValid(transform))
	{
		return {};
	}

	RenderObjectData data;
	data.entity = entity;
	data.transform = transform;
	data.materialID = materialID;
	data.mesh = mesh;
	data.transparent = transparent;
	return _renderObjects.Add(data);
}

JScene::EntityData* JScene::GetEntity(JEntityHandle handle)
{
	return _entities.Get(handle);
}

const JScene::EntityData* JScene::GetEntity(JEntityHandle handle) const
{
	return _entities.Get(handle);
}

JScene::TransformData* JScene::GetTransform(JTransformHandle handle)
{
	return _transforms.Get(handle);
}

const JScene::TransformData* JScene::GetTransform(JTransformHandle handle) const
{
	return _transforms.Get(handle);
}

JScene::CameraData* JScene::GetCamera(JCameraHandle handle)
{
	return _cameras.Get(handle);
}

const JScene::CameraData* JScene::GetCamera(JCameraHandle handle) const
{
	return _cameras.Get(handle);
}

JScene::LightData* JScene::GetLight(JLightHandle handle)
{
	return _lights.Get(handle);
}

const JScene::LightData* JScene::GetLight(JLightHandle handle) const
{
	return _lights.Get(handle);
}

JScene::RenderObjectData* JScene::GetRenderObject(JRenderObjectHandle handle)
{
	return _renderObjects.Get(handle);
}

const JScene::RenderObjectData* JScene::GetRenderObject(JRenderObjectHandle handle) const
{
	return _renderObjects.Get(handle);
}

void JScene::RotateCamera(JCameraHandle camera, float yawDelta, float pitchDelta)
{
	CameraData* cameraData = GetCamera(camera);
	if (cameraData == nullptr)
	{
		return;
	}

	TransformData* transform = GetTransform(cameraData->transform);
	if (transform == nullptr)
	{
		return;
	}

	transform->yaw += yawDelta * cameraData->rotateSpeed;
	transform->pitch += pitchDelta * cameraData->rotateSpeed;
}

void JScene::MoveCameraLocal(JCameraHandle camera, float forward, float right, float up, float deltaTime)
{
	CameraData* cameraData = GetCamera(camera);
	if (cameraData == nullptr)
	{
		return;
	}

	TransformData* transform = GetTransform(cameraData->transform);
	if (transform == nullptr)
	{
		return;
	}

	const XMVECTOR forwardVector = getForward(transform->yaw, transform->pitch);
	const XMVECTOR rightVector = getRight(transform->yaw);
	const XMVECTOR upVector = getUp(transform->yaw, transform->pitch);

	XMVECTOR position = XMVectorSet(transform->position.x, transform->position.y, transform->position.z, 1.0f);
	position += forwardVector * (forward * cameraData->moveSpeed * deltaTime);
	position += rightVector * (right * cameraData->moveSpeed * deltaTime);
	position += upVector * (up * cameraData->moveSpeed * deltaTime);

	XMFLOAT3 outPosition;
	XMStoreFloat3(&outPosition, position);
	transform->position = { outPosition.x, outPosition.y, outPosition.z };
}

XMMATRIX JScene::GetCameraViewMatrix(JCameraHandle camera) const
{
	const CameraData* cameraData = GetCamera(camera);
	if (cameraData == nullptr)
	{
		return XMMatrixIdentity();
	}

	const TransformData* transform = GetTransform(cameraData->transform);
	if (transform == nullptr)
	{
		return XMMatrixIdentity();
	}

	const XMVECTOR position = XMVectorSet(transform->position.x, transform->position.y, transform->position.z, 1.0f);
	const XMVECTOR forward = getForward(transform->yaw, transform->pitch);
	const XMVECTOR up = getUp(transform->yaw, transform->pitch);
	return XMMatrixLookAtLH(position, position + forward, up);
}

XMMATRIX JScene::GetCameraProjectionMatrix(JCameraHandle camera) const
{
	const CameraData* cameraData = GetCamera(camera);
	return cameraData != nullptr
		? XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), cameraData->aspectRatio, 0.1f, 1000.0f)
		: XMMatrixIdentity();
}

J_ENGINE_END
