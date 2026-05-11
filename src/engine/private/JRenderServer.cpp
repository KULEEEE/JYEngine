#include "engine/JRenderServer.h"

#include "engine/JGraphicResource.h"
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

bool JRenderServer::BuildGraphicResource(uint32 materialID, Render::JShader* shader, Render::JGraphicResource& outResource) const
{
	return _renderDB.BuildGraphicResource(materialID, shader, outResource);
}

J_ENGINE_END
