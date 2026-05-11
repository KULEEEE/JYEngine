#pragma once
#ifndef __J_CAMERA_COMPONENT_H__
#define __J_CAMERA_COMPONENT_H__

#include "engine/JObject.h"

J_ENGINE_BEGIN

class JCameraComponent : public JObject
{
public:
	JCameraComponent() = default;

	const JVec3& GetPosition() const { return _position; }
	void SetPosition(const JVec3& position) { _position = position; }
	float GetYaw() const { return _yaw; }
	float GetPitch() const { return _pitch; }

	float GetMoveSpeed() const { return _moveSpeed; }
	void SetMoveSpeed(float speed) { _moveSpeed = speed; }

	float GetRotateSpeed() const { return _rotateSpeed; }
	void SetRotateSpeed(float speed) { _rotateSpeed = speed; }

	XMMATRIX GetViewMatrix() const;
	XMMATRIX GetProjectionMatrix(float aspectRatio) const;

	void MoveLocal(float forward, float right, float up, float deltaTime);
	void Rotate(float yawDelta, float pitchDelta);

private:
	JVec3 _position = { 0.0f, 0.0f, -8.0f };
	float _yaw = 0.0f;
	float _pitch = 0.0f;
	float _moveSpeed = 0.1f;
	float _rotateSpeed = 1.0f;
};

J_ENGINE_END

#endif
