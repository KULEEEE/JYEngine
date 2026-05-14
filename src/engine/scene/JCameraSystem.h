#pragma once
#ifndef __J_CAMERA_SYSTEM_H__
#define __J_CAMERA_SYSTEM_H__

#include "engine/scene/JScene.h"

J_ENGINE_BEGIN

class JCameraSystem
{
public:
	JCameraSystem();
	~JCameraSystem();

	JCameraSystem(const JCameraSystem&) = delete;
	JCameraSystem& operator=(const JCameraSystem&) = delete;

	static JCameraSystem* Get();

	void SetRotate(JScene& scene, JCameraHandle camera, float x, float y, float z);
	void SetPosition(JScene& scene, JCameraHandle camera, float x, float y, float z);
	XMMATRIX GetViewMatrix(const JScene& scene, JCameraHandle camera) const;
	XMMATRIX GetProjectionMatrix(const JScene& scene, JCameraHandle camera) const;

private:
	static JCameraSystem* s_Instance;
};

J_ENGINE_END

#endif
