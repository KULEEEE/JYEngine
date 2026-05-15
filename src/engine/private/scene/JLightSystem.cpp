#include "engine/scene/JLightSystem.h"
#include "engine/core/JEngineContext.h"
#include "engine/render/JRenderContext.h"

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

JLightResource* JLightSystem::GetLightResource()
{
	return &_lightResource;
}

const JLightResource* JLightSystem::GetLightResource() const
{
	return &_lightResource;
}

void JLightSystem::SyncRenderDB(const JScene& scene)
{
	if (GetEngine() == nullptr || GetEngine()->GetRenderContext() == nullptr)
	{
		return;
	}

	struct LightConstants
	{
		JVec4 colorIntensity;
		JVec4 positionCount;
	};

	LightConstants constants{};
	constants.colorIntensity = JVec4(1.0f, 1.0f, 1.0f, 0.35f);
	constants.positionCount = JVec4(0.0f, 4.0f, -4.0f, 0.0f);

	uint32 lightCount = 0;
	for (const JScene::LightSlot& slot : scene.GetLightSlots())
	{
		if (!slot.active || !slot.data.active)
		{
			continue;
		}

		const JTransformHandle transformHandle = scene.GetTransformHandle(slot.data.entity);
		if (!transformHandle.IsValid())
		{
			continue;
		}

		const JScene::TransformData transform = scene.GetTransform(transformHandle);
		constants.colorIntensity = JVec4(slot.data.color.x, slot.data.color.y, slot.data.color.z, slot.data.intensity);
		constants.positionCount = JVec4(transform.translation.x, transform.translation.y, transform.translation.z, 1.0f);
		lightCount = 1;
		break;
	}

	_lightResource.lightCount = lightCount;
	_lightResource.colorIntensity = constants.colorIntensity;
	_lightResource.positionCount = constants.positionCount;
	if (_lightResource.lightBuffer == nullptr)
	{
		_lightResource.lightBuffer = GetEngine()->GetRenderContext()->CreateConstantBuffer(&constants, sizeof(constants));
		return;
	}

	GetEngine()->GetRenderContext()->UpdateConstantBuffer(_lightResource.lightBuffer, &constants, sizeof(constants));
}

J_ENGINE_END
