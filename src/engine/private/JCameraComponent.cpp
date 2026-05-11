#include "engine/JCameraComponent.h"

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

XMMATRIX JCameraComponent::GetViewMatrix() const
{
	const XMVECTOR position = XMVectorSet(_position.x, _position.y, _position.z, 1.0f);
	const XMVECTOR forward = getForward(_yaw, _pitch);
	const XMVECTOR up = getUp(_yaw, _pitch);
	const XMVECTOR target = position + forward;
	return XMMatrixLookAtLH(position, target, up);
}

XMMATRIX JCameraComponent::GetProjectionMatrix(float aspectRatio) const
{
	return XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspectRatio, 0.1f, 1000.0f);
}

void JCameraComponent::MoveLocal(float forwardAmount, float rightAmount, float upAmount, float deltaTime)
{
	const XMVECTOR forward = getForward(_yaw, _pitch);
	const XMVECTOR right = getRight(_yaw);
	const XMVECTOR up = getUp(_yaw, _pitch);

	XMVECTOR position = XMVectorSet(_position.x, _position.y, _position.z, 1.0f);
	position += forward * (forwardAmount * _moveSpeed * deltaTime);
	position += right * (rightAmount * _moveSpeed * deltaTime);
	position += up * (upAmount * _moveSpeed * deltaTime);

	XMFLOAT3 outPosition;
	XMStoreFloat3(&outPosition, position);
	_position = { outPosition.x, outPosition.y, outPosition.z };
}

void JCameraComponent::Rotate(float yawDelta, float pitchDelta)
{
	_yaw += yawDelta * _rotateSpeed;
	_pitch += pitchDelta * _rotateSpeed;
}

J_ENGINE_END
