#pragma once
#ifndef __J_RENDER_OBJECT_SYSTEM_H__
#define __J_RENDER_OBJECT_SYSTEM_H__

#include "engine/scene/JScene.h"

/*#include "engine/render/JRenderDB.h"*/ namespace J { namespace Engine { class JRenderDB; } }

J_ENGINE_BEGIN

class JRenderObjectSystem
{
public:
	JRenderObjectSystem();
	~JRenderObjectSystem();

	JRenderObjectSystem(const JRenderObjectSystem&) = delete;
	JRenderObjectSystem& operator=(const JRenderObjectSystem&) = delete;

	static JRenderObjectSystem* Get();

	void SyncRenderDB(const JScene& scene, JRenderDB& renderDB) const;

private:
	static JRenderObjectSystem* s_Instance;
};

J_ENGINE_END

#endif
