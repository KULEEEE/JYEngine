#include "engine/scene/JCameraSystem.h"

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

JCameraSystem* JCameraSystem::s_Instance = nullptr;

JCameraSystem::JCameraSystem()
{
	s_Instance = this;
}

JCameraSystem::~JCameraSystem()
{
	if (s_Instance == this)
	{
		s_Instance = nullptr;
	}
}

JCameraSystem* JCameraSystem::Get()
{
	return s_Instance;
}

void JCameraSystem::SetRotate(JScene& scene, JCameraHandle camera, float x, float y, float z)
{
	JScene::CameraData* cameraData = scene.GetCamera(camera);
	if (cameraData == nullptr)
	{
		return;
	}

	JScene::TransformData* transform = scene.GetTransform(cameraData->transform);
	if (transform == nullptr)
	{
		return;
	}

	transform->rotation = { x, y, z };
}

void JCameraSystem::SetPosition(JScene& scene, JCameraHandle camera, float x, float y, float z)
{
	JScene::CameraData* cameraData = scene.GetCamera(camera);
	if (cameraData == nullptr)
	{
		return;
	}

	JScene::TransformData* transform = scene.GetTransform(cameraData->transform);
	if (transform == nullptr)
	{
		return;
	}

	transform->translation = { x, y, z };
}

XMMATRIX JCameraSystem::GetViewMatrix(const JScene& scene, JCameraHandle camera) const
{
	const JScene::CameraData* cameraData = scene.GetCamera(camera);
	if (cameraData == nullptr)
	{
		return XMMatrixIdentity();
	}

	const JScene::TransformData* transform = scene.GetTransform(cameraData->transform);
	if (transform == nullptr)
	{
		return XMMatrixIdentity();
	}

	const XMVECTOR position = XMVectorSet(transform->translation.x, transform->translation.y, transform->translation.z, 1.0f);
	const XMVECTOR forward = getForward(transform->rotation.y, transform->rotation.x);
	const XMVECTOR up = getUp(transform->rotation.y, transform->rotation.x);
	return XMMatrixLookAtLH(position, position + forward, up);
}

XMMATRIX JCameraSystem::GetProjectionMatrix(const JScene& scene, JCameraHandle camera) const
{
	const JScene::CameraData* cameraData = scene.GetCamera(camera);
	if (cameraData == nullptr)
	{
		return XMMatrixIdentity();
	}

	const float nearPlane = std::max(0.001f, cameraData->nearP);
	const float farPlane = std::max(nearPlane + 0.001f, cameraData->farP);
	return XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), cameraData->aspectRatio, nearPlane, farPlane);
}

J_ENGINE_END
