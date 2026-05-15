#pragma once
#ifndef __J_LIGHT_SYSTEM_H__
#define __J_LIGHT_SYSTEM_H__

#include "engine/scene/JScene.h"
#include "engine/render/JRenderResource.h"

J_ENGINE_BEGIN

class JLightSystem
{
public:
	JLightSystem();
	~JLightSystem();

	JLightSystem(const JLightSystem&) = delete;
	JLightSystem& operator=(const JLightSystem&) = delete;

	static JLightSystem* Get();

	void SyncRenderDB(const JScene& scene);
	JLightResource* GetLightResource();
	const JLightResource* GetLightResource() const;

private:
	static JLightSystem* s_Instance;
	JLightResource _lightResource;
};

J_ENGINE_END

#endif
