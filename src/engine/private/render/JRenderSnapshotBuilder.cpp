#include "engine/render/JRenderSnapshotBuilder.h"

J_ENGINE_BEGIN

namespace
{
	XMVECTOR getForward(float yaw, float pitch)
	{
		const float cosPitch = cosf(pitch);
		return XMVector3Normalize(XMVectorSet(
			sinf(yaw) * cosPitch,
			sinf(pitch),
			cosf(yaw) * cosPitch,
			0.0f));
	}

	XMVECTOR getForwardXZ(float yaw)
	{
		return XMVector3Normalize(XMVectorSet(sinf(yaw), 0.0f, cosf(yaw), 0.0f));
	}

	XMVECTOR getRight(float yaw)
	{
		const XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
		return XMVector3Normalize(XMVector3Cross(worldUp, getForwardXZ(yaw)));
	}

	XMVECTOR getUp(float yaw, float pitch)
	{
		const XMVECTOR forward = getForward(yaw, pitch);
		const XMVECTOR right = getRight(yaw);
		return XMVector3Normalize(XMVector3Cross(forward, right));
	}

	XMMATRIX makeWorldMatrix(const JScene::TransformData& transform)
	{
		const XMMATRIX scale = XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z);
		const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(transform.rotation.x, transform.rotation.y, transform.rotation.z);
		const XMMATRIX translation = XMMatrixTranslation(transform.translation.x, transform.translation.y, transform.translation.z);
		return scale * rotation * translation;
	}

	JVec4 getCameraWorldPosition(const JScene& scene, JCameraHandle camera)
	{
		const JScene::CameraData* cameraData = scene.GetCamera(camera);
		if (cameraData == nullptr)
		{
			return { 0.0f, 0.0f, 0.0f, 1.0f };
		}

		const JTransformHandle transformHandle = scene.GetTransformHandle(cameraData->entity);
		if (!transformHandle.IsValid())
		{
			return { 0.0f, 0.0f, 0.0f, 1.0f };
		}

		const JScene::TransformData transform = scene.GetTransform(transformHandle);
		return { transform.translation.x, transform.translation.y, transform.translation.z, 1.0f };
	}

	XMMATRIX makeViewMatrix(const JScene& scene, JCameraHandle camera)
	{
		const JScene::CameraData* cameraData = scene.GetCamera(camera);
		if (cameraData == nullptr)
		{
			return XMMatrixIdentity();
		}

		const JTransformHandle transformHandle = scene.GetTransformHandle(cameraData->entity);
		if (!transformHandle.IsValid())
		{
			return XMMatrixIdentity();
		}

		const JScene::TransformData transform = scene.GetTransform(transformHandle);
		const XMVECTOR position = XMVectorSet(transform.translation.x, transform.translation.y, transform.translation.z, 1.0f);
		const XMVECTOR forward = getForward(transform.rotation.y, transform.rotation.x);
		const XMVECTOR up = getUp(transform.rotation.y, transform.rotation.x);
		return XMMatrixLookAtLH(position, position + forward, up);
	}

	XMMATRIX makeProjectionMatrix(const JScene& scene, JCameraHandle camera)
	{
		const JScene::CameraData* cameraData = scene.GetCamera(camera);
		if (cameraData == nullptr)
		{
			return XMMatrixIdentity();
		}

		const float nearPlane = std::max(0.001f, cameraData->nearP);
		const float farPlane = std::max(nearPlane + 0.001f, cameraData->farP);
		return XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), cameraData->aspectRatio, farPlane, nearPlane);
	}

	uint64 makeCameraKey(JCameraHandle camera)
	{
		return (static_cast<uint64>(camera.generation) << 32) | camera.index;
	}

	uint64 makeTransformKey(JTransformHandle transform)
	{
		return (static_cast<uint64>(transform.generation) << 32) | transform.index;
	}
}

void JRenderSnapshotBuilder::Build(const Input& input, JFrameSnapshot& outSnapshot, Result& outResult)
{
	resetOutput(outSnapshot, outResult);
	if (input.scene == nullptr || input.cameras == nullptr)
	{
		return;
	}

	collectActiveSceneResourceKeys(*input.scene, *input.cameras, outResult);
	buildCameraSnapshots(*input.scene, *input.cameras, outSnapshot);
	buildTransformSnapshots(*input.scene, outSnapshot);
	buildLightSnapshots(*input.scene, outSnapshot);
}

void JRenderSnapshotBuilder::resetOutput(JFrameSnapshot& outSnapshot, Result& outResult)
{
	outSnapshot.cameras.clear();
	outSnapshot.transforms.clear();
	outSnapshot.lights.clear();
	outResult.activeCameraKeys.clear();
	outResult.activeTransformKeys.clear();
	outResult.activeMeshes.clear();
}

void JRenderSnapshotBuilder::collectActiveSceneResourceKeys(const JScene& scene, const std::vector<JCameraHandle>& cameras, Result& outResult)
{
	const std::vector<JTransformPool::SlotType>& transformSlots = scene.GetTransformSlots();
	for (uint32 transformIndex = 0; transformIndex < transformSlots.size(); ++transformIndex)
	{
		const JTransformPool::SlotType& slot = transformSlots[transformIndex];
		if (slot.active)
		{
			outResult.activeTransformKeys.insert(makeTransformKey({ transformIndex, slot.generation }));
		}
	}

	for (JCameraHandle camera : cameras)
	{
		if (scene.GetCamera(camera) != nullptr)
		{
			outResult.activeCameraKeys.insert(makeCameraKey(camera));
		}
	}
}

void JRenderSnapshotBuilder::buildCameraSnapshots(const JScene& scene, const std::vector<JCameraHandle>& cameras, JFrameSnapshot& outSnapshot)
{
	for (JCameraHandle camera : cameras)
	{
		if (scene.GetCamera(camera) == nullptr)
		{
			continue;
		}

		const XMMATRIX viewProjection = makeViewMatrix(scene, camera) * makeProjectionMatrix(scene, camera);
		const XMMATRIX inverseViewProjection = XMMatrixInverse(nullptr, viewProjection);
		outSnapshot.cameras.push_back({ camera, viewProjection, inverseViewProjection, getCameraWorldPosition(scene, camera) });
	}
}

void JRenderSnapshotBuilder::buildTransformSnapshots(JScene& scene, JFrameSnapshot& outSnapshot)
{
	const std::vector<JTransformPool::SlotType>& transformSlots = scene.GetTransformSlots();
	const std::vector<uint32> dirtyTransformIndices = scene.ConsumeDirtyTransformIndices();
	for (uint32 transformIndex : dirtyTransformIndices)
	{
		if (transformIndex >= transformSlots.size())
		{
			continue;
		}

		const JTransformPool::SlotType& slot = transformSlots[transformIndex];
		if (!slot.active)
		{
			continue;
		}

		const JTransformHandle transformHandle = { transformIndex, slot.generation };
		const XMMATRIX world = makeWorldMatrix(scene.GetTransform(transformHandle));
		outSnapshot.transforms.push_back({ transformHandle, world });
	}
}

void JRenderSnapshotBuilder::buildLightSnapshots(const JScene& scene, JFrameSnapshot& outSnapshot)
{
	JLightSnapshot snapshot{};
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
		JLightSnapshot::Item item{};
		item.colorIntensity = JVec4(slot.data.color.x, slot.data.color.y, slot.data.color.z, slot.data.intensity);
		if (slot.data.type == JLightType::Directional)
		{
			// directional: transform rotation의 +Z(forward)가 빛이 진행하는 방향.
			// position.xyz에 방향을 싣고 w=0을 directional 마커로 쓴다. (point는 w=range>0)
			const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(transform.rotation.x, transform.rotation.y, transform.rotation.z);
			const XMVECTOR direction = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotation));
			XMFLOAT3 directionValue;
			XMStoreFloat3(&directionValue, direction);
			item.position = JVec4(directionValue.x, directionValue.y, directionValue.z, 0.0f);
		}
		else
		{
			// position.w에 light range를 실어 보낸다. (shader에서 거리 감쇠 윈도잉에 사용)
			item.position = JVec4(transform.translation.x, transform.translation.y, transform.translation.z, slot.data.range);
		}
		snapshot.items.push_back(item);
	}

	outSnapshot.lights.push_back(snapshot);
}

J_ENGINE_END
