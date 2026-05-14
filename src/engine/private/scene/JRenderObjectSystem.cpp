#include "engine/scene/JRenderObjectSystem.h"

#include "engine/render/JRenderDB.h"

J_ENGINE_BEGIN

JRenderObjectSystem* JRenderObjectSystem::s_Instance = nullptr;

JRenderObjectSystem::JRenderObjectSystem()
{
	s_Instance = this;
}

JRenderObjectSystem::~JRenderObjectSystem()
{
	if (s_Instance == this)
	{
		s_Instance = nullptr;
	}
}

JRenderObjectSystem* JRenderObjectSystem::Get()
{
	return s_Instance;
}

void JRenderObjectSystem::SyncRenderDB(const JScene& scene, JRenderDB& renderDB) const
{
	for (const JScene::RenderObjectSlot& slot : scene.GetRenderObjectSlots())
	{
		if (!slot.active || !slot.data.active || !slot.data.visible || slot.data.mesh == nullptr)
		{
			continue;
		}

		renderDB.GetOrCreateMeshResource(slot.data.mesh);
	}
}

J_ENGINE_END
