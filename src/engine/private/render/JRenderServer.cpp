#include "engine/render/JRenderServer.h"

#include "engine/render/JGraphicResource.h"
#include "engine/asset/JMaterial.h"
#include "engine/render/JMaterialResource.h"
#include "engine/scene/JCameraSystem.h"
#include "engine/scene/JTransformSystem.h"
#include "engine/scene/JLightSystem.h"
#include "engine/scene/JRenderObjectSystem.h"

J_ENGINE_BEGIN

uint64 JRenderServer::MakeCameraKey(JCameraHandle camera)
{
	return (static_cast<uint64>(camera.generation) << 32) | camera.index;
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

void JRenderServer::SyncScene(const JScene& scene)
{
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

			const JCameraSystem* cameraSystem = JCameraSystem::Get();
			const XMMATRIX viewProjection = cameraSystem != nullptr
				? cameraSystem->GetViewMatrix(scene, record.camera) * cameraSystem->GetProjectionMatrix(scene, record.camera)
				: XMMatrixIdentity();
			_renderDB.SyncCamera(record.camera, viewProjection, record.perFrameBuffer);
			break;
		}
	}

	_dirtyCameraKeys.clear();

	if (const JTransformSystem* transformSystem = JTransformSystem::Get())
	{
		transformSystem->SyncRenderDB(scene, _renderDB);
	}

	if (const JRenderObjectSystem* renderObjectSystem = JRenderObjectSystem::Get())
	{
		renderObjectSystem->SyncRenderDB(scene, _renderDB);
	}

	if (const JLightSystem* lightSystem = JLightSystem::Get())
	{
		lightSystem->SyncRenderDB(scene, _renderDB);
	}
}

bool JRenderServer::BuildFrameDesc(const JScene& scene, JRenderTarget* renderTarget, const JColor& clearColor, const Render::JViewport& viewport, const D3D12_RECT& scissorRect, JRenderer::FrameDesc& outFrameDesc) const
{
	const JCameraHandle primaryCamera = scene.GetPrimaryCamera();
	if (!primaryCamera.IsValid())
	{
		return false;
	}

	outFrameDesc = {};
	outFrameDesc.camera = primaryCamera;
	outFrameDesc.renderTarget = renderTarget;
	outFrameDesc.clearColor = clearColor;
	outFrameDesc.viewport = viewport;
	outFrameDesc.scissorRect = scissorRect;

	for (const JScene::LightSlot& lightSlot : scene.GetLightSlots())
	{
		if (!lightSlot.active || !lightSlot.data.active)
		{
			continue;
		}

		outFrameDesc.lights.push_back({ static_cast<uint32>(&lightSlot - scene.GetLightSlots().data()), lightSlot.generation });
	}

	for (const JScene::RenderObjectSlot& slot : scene.GetRenderObjectSlots())
	{
		if (!slot.active || !slot.data.active || !slot.data.visible || slot.data.mesh == nullptr)
		{
			continue;
		}

		JRenderer::DrawItem drawItem;
		drawItem.entity = slot.data.entity;
		drawItem.transform = slot.data.transform;
		drawItem.renderObject = { static_cast<uint32>(&slot - scene.GetRenderObjectSlots().data()), slot.generation };
		drawItem.materialID = slot.data.materialID;
		drawItem.mesh = slot.data.mesh;
		drawItem.transparent = slot.data.transparent;

		if (drawItem.transparent)
		{
			outFrameDesc.transparentDrawItems.push_back(drawItem);
		}
		else
		{
			outFrameDesc.opaqueDrawItems.push_back(drawItem);
		}
	}

	return true;
}

bool JRenderServer::BuildGraphicResource(uint32 materialID, Render::JShader* shader, Render::JGraphicResource& outResource) const
{
	return _renderDB.BuildGraphicResource(materialID, shader, outResource);
}

J_ENGINE_END
