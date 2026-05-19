#include "engine/render/JRenderServer.h"

#include "engine/asset/JMaterial.h"

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
		return XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), cameraData->aspectRatio, nearPlane, farPlane);
	}

	uint64 makeTransformKey(JTransformHandle transform)
	{
		return (static_cast<uint64>(transform.generation) << 32) | transform.index;
	}
}

uint64 JRenderServer::MakeCameraKey(JCameraHandle camera)
{
	return (static_cast<uint64>(camera.generation) << 32) | camera.index;
}

JRenderServer::~JRenderServer()
{
}

uint32 JRenderServer::FindMaterialIndex(uint32 materialID) const
{
	const auto iter = _materialIndexMap.find(materialID);
	return iter == _materialIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
}

JMaterial* JRenderServer::FindMaterial(uint32 materialID) const
{
	const uint32 index = FindMaterialIndex(materialID);
	return index == static_cast<uint32>(-1) ? nullptr : _materials[index].source;
}

uint32 JRenderServer::FindCameraIndex(JCameraHandle camera) const
{
	const auto iter = _cameraIndexMap.find(MakeCameraKey(camera));
	return iter == _cameraIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
}

JRenderServer::CameraRecord* JRenderServer::FindCameraRecord(JCameraHandle camera)
{
	const uint32 index = FindCameraIndex(camera);
	return index == static_cast<uint32>(-1) ? nullptr : &_cameras[index];
}

const JRenderServer::CameraRecord* JRenderServer::FindCameraRecord(JCameraHandle camera) const
{
	const uint32 index = FindCameraIndex(camera);
	return index == static_cast<uint32>(-1) ? nullptr : &_cameras[index];
}

void JRenderServer::RegisterMaterial(JMaterial* material)
{
	if (material == nullptr)
	{
		return;
	}

	if (FindMaterialIndex(material->instanceID) != static_cast<uint32>(-1))
	{
		return;
	}

	const uint32 newIndex = static_cast<uint32>(_materials.size());
	_materials.push_back({ material->instanceID, material });
	_materialIndexMap[material->instanceID] = newIndex;
	MarkMaterialDirty(material);
}

void JRenderServer::UnregisterMaterial(uint32 materialID)
{
	const uint32 index = FindMaterialIndex(materialID);
	if (index != static_cast<uint32>(-1))
	{
		const uint32 lastIndex = static_cast<uint32>(_materials.size() - 1);
		if (index != lastIndex)
		{
			_materials[index] = _materials[lastIndex];
			_materialIndexMap[_materials[index].materialID] = index;
		}
		_materials.pop_back();
		_materialIndexMap.erase(materialID);
	}

	for (auto iter = _dirtyMaterialIDs.begin(); iter != _dirtyMaterialIDs.end();)
	{
		if (*iter == materialID)
		{
			iter = _dirtyMaterialIDs.erase(iter);
			continue;
		}
		++iter;
	}

	_renderDB.RemoveMaterialResource(materialID);
}

void JRenderServer::RegisterCamera(JCameraHandle camera, Render::JConstantBuffer* perFrameBuffer)
{
	if (!camera.IsValid())
	{
		return;
	}

	if (FindCameraIndex(camera) != static_cast<uint32>(-1))
	{
		CameraRecord* record = FindCameraRecord(camera);
		record->perFrameBuffer = perFrameBuffer;
		MarkCameraDirty(camera);
		return;
	}

	const uint32 newIndex = static_cast<uint32>(_cameras.size());
	_cameras.push_back({ camera, perFrameBuffer });
	_cameraIndexMap[MakeCameraKey(camera)] = newIndex;
	MarkCameraDirty(camera);
}

void JRenderServer::UnregisterCamera(JCameraHandle camera)
{
	const uint32 index = FindCameraIndex(camera);
	if (index != static_cast<uint32>(-1))
	{
		const uint32 lastIndex = static_cast<uint32>(_cameras.size() - 1);
		if (index != lastIndex)
		{
			_cameras[index] = _cameras[lastIndex];
			_cameraIndexMap[MakeCameraKey(_cameras[index].camera)] = index;
		}
		_cameras.pop_back();
		_cameraIndexMap.erase(MakeCameraKey(camera));
	}

	for (auto iter = _dirtyCameraKeys.begin(); iter != _dirtyCameraKeys.end();)
	{
		if (*iter == MakeCameraKey(camera))
		{
			iter = _dirtyCameraKeys.erase(iter);
			continue;
		}
		++iter;
	}

	_renderDB.RemoveCameraResource(camera);
}

void JRenderServer::MarkMaterialDirty(JMaterial* material)
{
	if (material == nullptr)
	{
		return;
	}

	material->MarkDirty();

	for (uint32 dirtyMaterialID : _dirtyMaterialIDs)
	{
		if (dirtyMaterialID == material->instanceID)
		{
			return;
		}
	}

	_dirtyMaterialIDs.push_back(material->instanceID);
}

void JRenderServer::MarkCameraDirty(JCameraHandle camera)
{
	if (!camera.IsValid())
	{
		return;
	}

	const uint64 cameraKey = MakeCameraKey(camera);
	for (uint64 dirtyCameraKey : _dirtyCameraKeys)
	{
		if (dirtyCameraKey == cameraKey)
		{
			return;
		}
	}

	_dirtyCameraKeys.push_back(cameraKey);
}

void JRenderServer::Sync()
{
	for (uint32 materialID : _dirtyMaterialIDs)
	{
		JMaterial* material = FindMaterial(materialID);
		if (material == nullptr)
		{
			continue;
		}

		_renderDB.SyncMaterial(*material);
		material->ClearDirty();
	}

	_dirtyMaterialIDs.clear();
}

void JRenderServer::SyncScene(JScene& scene)
{
	_primaryCamera = scene.GetPrimaryCamera();
	_frameSnapshot.cameras.clear();
	_frameSnapshot.transforms.clear();
	_frameSnapshot.lights.clear();
	_frameSnapshot.renderObjects.clear();
	_frameSnapshot.opaqueDrawItems.clear();
	_frameSnapshot.transparentDrawItems.clear();

	std::unordered_set<uint64> activeCameraKeys;
	std::unordered_set<uint64> activeTransformKeys;
	std::unordered_set<const JMesh*> activeMeshes;

	const std::vector<JTransformPool::SlotType>& transformSlots = scene.GetTransformSlots();
	for (uint32 transformIndex = 0; transformIndex < transformSlots.size(); ++transformIndex)
	{
		const JTransformPool::SlotType& slot = transformSlots[transformIndex];
		if (!slot.active)
		{
			continue;
		}

		activeTransformKeys.insert(makeTransformKey({ transformIndex, slot.generation }));
	}

	for (uint64 cameraKey : _dirtyCameraKeys)
	{
		for (const CameraRecord& record : _cameras)
		{
			if (MakeCameraKey(record.camera) != cameraKey)
			{
				continue;
			}

			if (scene.GetCamera(record.camera) == nullptr)
			{
				break;
			}

			activeCameraKeys.insert(MakeCameraKey(record.camera));
			const XMMATRIX viewProjection = makeViewMatrix(scene, record.camera) * makeProjectionMatrix(scene, record.camera);
			_frameSnapshot.cameras.push_back({ record.camera, viewProjection, record.perFrameBuffer });
			_renderDB.SyncCamera(record.camera, viewProjection, record.perFrameBuffer);
			break;
		}
	}

	_dirtyCameraKeys.clear();

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

		const JTransformHandle transformHandle = {
			transformIndex,
			slot.generation
		};
		const XMMATRIX world = makeWorldMatrix(scene.GetTransform(transformHandle));
		_frameSnapshot.transforms.push_back({ transformHandle, world });
		_renderDB.SyncTransform(transformHandle, world);
	}

	for (const JScene::RenderObjectSlot& slot : scene.GetRenderObjectSlots())
	{
		if (!slot.active || !slot.data.active || !slot.data.visible || slot.data.mesh == nullptr)
		{
			continue;
		}

		activeMeshes.insert(slot.data.mesh);
		_frameSnapshot.renderObjects.push_back({
			slot.data.entity,
			{ static_cast<uint32>(&slot - scene.GetRenderObjectSlots().data()), slot.generation },
			slot.data.materialID,
			slot.data.mesh,
			slot.data.transparent,
			slot.data.visible,
			slot.data.active
		});
		_renderDB.GetOrCreateMeshResource(slot.data.mesh);
	}

	for (const JRenderObjectSnapshot& snapshot : _frameSnapshot.renderObjects)
	{
		if (!snapshot.active || !snapshot.visible || snapshot.mesh == nullptr)
		{
			continue;
		}

		const JTransformHandle transform = scene.GetTransformHandle(snapshot.entity);

		JRenderer::DrawItem drawItem;
		drawItem.entity = snapshot.entity;
		drawItem.renderObject = snapshot.renderObject;
		drawItem.materialID = snapshot.materialID;
		drawItem.mesh = snapshot.mesh;
		drawItem.meshResource = _renderDB.FindMeshResource(snapshot.mesh);
		drawItem.materialResource = _renderDB.FindMaterialResource(snapshot.materialID);
		drawItem.transformResource = _renderDB.FindTransformResource(transform);
		drawItem.transparent = snapshot.transparent;

		if (drawItem.meshResource == nullptr || drawItem.materialResource == nullptr || drawItem.transformResource == nullptr)
		{
			continue;
		}

		if (drawItem.transparent)
		{
			_frameSnapshot.transparentDrawItems.push_back(drawItem);
		}
		else
		{
			_frameSnapshot.opaqueDrawItems.push_back(drawItem);
		}
	}

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
			JLightSnapshotItem item{};
			item.colorIntensity = JVec4(slot.data.color.x, slot.data.color.y, slot.data.color.z, slot.data.intensity);
			item.position = JVec4(transform.translation.x, transform.translation.y, transform.translation.z, 1.0f);
			snapshot.items.push_back(item);
		}

		_frameSnapshot.lights.push_back(snapshot);
		_renderDB.SyncLight(snapshot);
	}

	for (const CameraRecord& record : _cameras)
	{
		if (scene.GetCamera(record.camera) != nullptr)
		{
			activeCameraKeys.insert(MakeCameraKey(record.camera));
		}
	}

	_renderDB.PruneUnusedSceneResources(activeCameraKeys, activeTransformKeys, activeMeshes);
}

bool JRenderServer::BuildFrameDesc(JRenderTarget* renderTarget, const JColor& clearColor, const Render::JViewport& viewport, const D3D12_RECT& scissorRect, JRenderer::FrameDesc& outFrameDesc) const
{
	if (!_primaryCamera.IsValid())
	{
		return false;
	}

	outFrameDesc = {};
	outFrameDesc.camera = _primaryCamera;
	outFrameDesc.renderTarget = renderTarget;
	outFrameDesc.clearColor = clearColor;
	outFrameDesc.viewport = viewport;
	outFrameDesc.scissorRect = scissorRect;
	outFrameDesc.opaqueDrawItems = _frameSnapshot.opaqueDrawItems;
	outFrameDesc.transparentDrawItems = _frameSnapshot.transparentDrawItems;

	return true;
}

J_ENGINE_END
