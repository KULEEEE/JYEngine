#include "engine/JRenderServer.h"

#include "engine/JGraphicResource.h"
#include "engine/JCameraComponent.h"
#include "engine/asset/JMaterial.h"
#include "engine/JMaterialResource.h"

J_ENGINE_BEGIN

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

uint32 JRenderServer::FindCameraIndex(uint32 cameraID) const
{
	const auto iter = _cameraIndexMap.find(cameraID);
	return iter == _cameraIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
}

JRenderServer::CameraRecord* JRenderServer::FindCameraRecord(uint32 cameraID)
{
	const uint32 index = FindCameraIndex(cameraID);
	return index == static_cast<uint32>(-1) ? nullptr : &_cameras[index];
}

const JRenderServer::CameraRecord* JRenderServer::FindCameraRecord(uint32 cameraID) const
{
	const uint32 index = FindCameraIndex(cameraID);
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

void JRenderServer::RegisterCamera(JCameraComponent* camera, Render::JConstantBuffer* perFrameBuffer, float aspectRatio)
{
	if (camera == nullptr)
	{
		return;
	}

	if (FindCameraIndex(camera->instanceID) != static_cast<uint32>(-1))
	{
		CameraRecord* record = FindCameraRecord(camera->instanceID);
		record->perFrameBuffer = perFrameBuffer;
		record->aspectRatio = aspectRatio;
		MarkCameraDirty(camera);
		return;
	}

	const uint32 newIndex = static_cast<uint32>(_cameras.size());
	_cameras.push_back({ camera->instanceID, camera, perFrameBuffer, aspectRatio });
	_cameraIndexMap[camera->instanceID] = newIndex;
	MarkCameraDirty(camera);
}

void JRenderServer::UnregisterCamera(uint32 cameraID)
{
	const uint32 index = FindCameraIndex(cameraID);
	if (index != static_cast<uint32>(-1))
	{
		const uint32 lastIndex = static_cast<uint32>(_cameras.size() - 1);
		if (index != lastIndex)
		{
			_cameras[index] = _cameras[lastIndex];
			_cameraIndexMap[_cameras[index].cameraID] = index;
		}
		_cameras.pop_back();
		_cameraIndexMap.erase(cameraID);
	}

	for (auto iter = _dirtyCameraIDs.begin(); iter != _dirtyCameraIDs.end();)
	{
		if (*iter == cameraID)
		{
			iter = _dirtyCameraIDs.erase(iter);
			continue;
		}
		++iter;
	}

	_renderDB.RemoveCameraResource(cameraID);
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

void JRenderServer::MarkCameraDirty(JCameraComponent* camera)
{
	if (camera == nullptr)
	{
		return;
	}

	for (uint32 dirtyCameraID : _dirtyCameraIDs)
	{
		if (dirtyCameraID == camera->instanceID)
		{
			return;
		}
	}

	_dirtyCameraIDs.push_back(camera->instanceID);
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

	for (uint32 cameraID : _dirtyCameraIDs)
	{
		const CameraRecord* record = FindCameraRecord(cameraID);
		if (record == nullptr || record->source == nullptr)
		{
			continue;
		}

		const XMMATRIX viewProjection = record->source->GetViewMatrix() * record->source->GetProjectionMatrix(record->aspectRatio);
		_renderDB.SyncCamera(record->cameraID, viewProjection, record->perFrameBuffer);
	}

	_dirtyCameraIDs.clear();
}

bool JRenderServer::BuildGraphicResource(uint32 materialID, Render::JShader* shader, Render::JGraphicResource& outResource) const
{
	return _renderDB.BuildGraphicResource(materialID, shader, outResource);
}

J_ENGINE_END
