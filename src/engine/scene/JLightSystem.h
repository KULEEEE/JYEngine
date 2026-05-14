#pragma once
#ifndef __J_LIGHT_SYSTEM_H__
#define __J_LIGHT_SYSTEM_H__

#include "engine/scene/JScene.h"

/*#include "engine/render/JRenderDB.h"*/ namespace J { namespace Engine { class JRenderDB; } }

J_ENGINE_BEGIN

class JLightSystem
{
public:
	JLightSystem();
	~JLightSystem();

	JLightSystem(const JLightSystem&) = delete;
	JLightSystem& operator=(const JLightSystem&) = delete;

	static JLightSystem* Get();

	void SyncRenderDB(const JScene& scene, JRenderDB& renderDB) const;

private:
	static JLightSystem* s_Instance;
};

J_ENGINE_END

#endif
