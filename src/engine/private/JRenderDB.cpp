#include "engine/JRenderDB.h"

#include "engine/JGraphicResource.h"
#include "engine/asset/JMaterial.h"
#include "engine/JMaterialResource.h"

J_ENGINE_BEGIN

JRenderDB::~JRenderDB()
{
	Clear();
}

JMaterialResource& JRenderDB::GetOrCreateMaterialResource(uint32 materialID)
{
	const auto iter = _materialIndexMap.find(materialID);
	if (iter != _materialIndexMap.end())
	{
		return _materialResources[iter->second].resource;
	}

	MaterialResourceRecord record;
	record.materialID = materialID;
	record.resource = JMaterialResource(materialID);

	const uint32 newIndex = static_cast<uint32>(_materialResources.size());
	_materialResources.push_back(record);
	_materialIndexMap[materialID] = newIndex;
	return _materialResources.back().resource;
}

uint32 JRenderDB::FindMaterialResourceIndex(uint32 materialID) const
{
	const auto iter = _materialIndexMap.find(materialID);
	return iter == _materialIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
}

JMaterialResource* JRenderDB::FindMaterialResource(uint32 materialID)
{
	const uint32 index = FindMaterialResourceIndex(materialID);
	return index == static_cast<uint32>(-1) ? nullptr : &_materialResources[index].resource;
}

const JMaterialResource* JRenderDB::FindMaterialResource(uint32 materialID) const
{
	const uint32 index = FindMaterialResourceIndex(materialID);
	return index == static_cast<uint32>(-1) ? nullptr : &_materialResources[index].resource;
}

void JRenderDB::SyncMaterial(const JMaterial& material)
{
	JMaterialResource& resource = GetOrCreateMaterialResource(material.instanceID);
	resource.ClearBindings();

	for (const JMaterial::ConstantBufferParam& param : material.GetConstantBuffers())
	{
		resource.SetConstantBuffer(param.name, param.buffer);
	}

	for (const JMaterial::TextureParam& param : material.GetTextures())
	{
		resource.SetTexture(param.name, param.texture);
	}

	resource.ClearDirty();
}

void JRenderDB::RemoveMaterialResource(uint32 materialID)
{
	const uint32 index = FindMaterialResourceIndex(materialID);
	if (index == static_cast<uint32>(-1))
	{
		return;
	}

	const uint32 lastIndex = static_cast<uint32>(_materialResources.size() - 1);
	if (index != lastIndex)
	{
		_materialResources[index] = _materialResources[lastIndex];
		_materialIndexMap[_materialResources[index].materialID] = index;
	}

	_materialResources.pop_back();
	_materialIndexMap.erase(materialID);
}

void JRenderDB::Clear()
{
	_materialResources.clear();
	_materialIndexMap.clear();
}

bool JRenderDB::BuildGraphicResource(uint32 materialID, Render::JShader* shader, Render::JGraphicResource& outResource) const
{
	const JMaterialResource* materialResource = FindMaterialResource(materialID);
	if (materialResource == nullptr || shader == nullptr)
	{
		return false;
	}

	outResource.SetShader(shader);

	for (const JMaterialResource::ConstantBufferEntry& entry : materialResource->GetConstantBuffers())
	{
		outResource.SetConstantBuffer(entry.nameHash, entry.buffer, entry.name);
	}

	for (const JMaterialResource::TextureEntry& entry : materialResource->GetTextures())
	{
		outResource.SetTexture(entry.nameHash, entry.texture, entry.name);
	}

	return true;
}

J_ENGINE_END
