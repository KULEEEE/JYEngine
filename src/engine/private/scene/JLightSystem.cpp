#include "engine/scene/JLightSystem.h"

#include "engine/render/JRenderDB.h"

J_ENGINE_BEGIN

JLightSystem* JLightSystem::s_Instance = nullptr;

JLightSystem::JLightSystem()
{
	s_Instance = this;
}

JLightSystem::~JLightSystem()
{
	if (s_Instance == this)
	{
		s_Instance = nullptr;
	}
}

JLightSystem* JLightSystem::Get()
{
	return s_Instance;
}

void JLightSystem::SyncRenderDB(const JScene& scene, JRenderDB& renderDB) const
{
	renderDB.SyncLights(scene);
}

J_ENGINE_END
