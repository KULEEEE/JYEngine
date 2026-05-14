#include "engine/scene/JTransformSystem.h"

#include "engine/render/JRenderDB.h"

J_ENGINE_BEGIN

JTransformSystem* JTransformSystem::s_Instance = nullptr;

JTransformSystem::JTransformSystem()
{
	s_Instance = this;
}

JTransformSystem::~JTransformSystem()
{
	if (s_Instance == this)
	{
		s_Instance = nullptr;
	}
}

JTransformSystem* JTransformSystem::Get()
{
	return s_Instance;
}

XMMATRIX JTransformSystem::MakeWorldMatrix(const JScene::TransformData& transform) const
{
	const XMMATRIX scale = XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z);
	const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(transform.rotation.x, transform.rotation.y, transform.rotation.z);
	const XMMATRIX translation = XMMatrixTranslation(transform.translation.x, transform.translation.y, transform.translation.z);
	return scale * rotation * translation;
}

void JTransformSystem::SyncRenderDB(const JScene& scene, JRenderDB& renderDB) const
{
	const std::vector<JScene::TransformSlot>& slots = scene.GetTransformSlots();
	for (const JScene::TransformSlot& slot : slots)
	{
		if (!slot.active)
		{
			continue;
		}

		const JTransformHandle transformHandle = {
			static_cast<uint32>(&slot - slots.data()),
			slot.generation
		};
		renderDB.SyncTransform(transformHandle, MakeWorldMatrix(slot.data));
	}
}

J_ENGINE_END
