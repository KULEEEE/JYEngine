#pragma once
#ifndef __J_TRANSFORM_SYSTEM_H__
#define __J_TRANSFORM_SYSTEM_H__

#include "engine/scene/JScene.h"

/*#include "engine/render/JRenderDB.h"*/ namespace J { namespace Engine { class JRenderDB; } }

J_ENGINE_BEGIN

class JTransformSystem
{
public:
	JTransformSystem();
	~JTransformSystem();

	JTransformSystem(const JTransformSystem&) = delete;
	JTransformSystem& operator=(const JTransformSystem&) = delete;

	static JTransformSystem* Get();

	XMMATRIX MakeWorldMatrix(const JScene::TransformData& transform) const;
	void SyncRenderDB(const JScene& scene, JRenderDB& renderDB) const;

private:
	static JTransformSystem* s_Instance;
};

J_ENGINE_END

#endif
