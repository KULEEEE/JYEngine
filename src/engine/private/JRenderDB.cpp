#include "engine/JRenderDB.h"

#include "engine/JGraphicResource.h"
#include "engine/JRenderContext.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"
#include "engine/JMaterialResource.h"

J_ENGINE_BEGIN

JRenderDB::~JRenderDB()
{
	Clear();
}

void JRenderDB::Initialize(Render::JRenderContext* renderContext)
{
	_renderContext = renderContext;
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

JRenderDB::CameraResource& JRenderDB::GetOrCreateCameraResource(uint32 cameraID)
{
	const auto iter = _cameraIndexMap.find(cameraID);
	if (iter != _cameraIndexMap.end())
	{
		return _cameraResources[iter->second].resource;
	}

	CameraResourceRecord record;
	record.cameraID = cameraID;
	record.resource.cameraID = cameraID;

	const uint32 newIndex = static_cast<uint32>(_cameraResources.size());
	_cameraResources.push_back(record);
	_cameraIndexMap[cameraID] = newIndex;
	return _cameraResources.back().resource;
}

uint32 JRenderDB::FindCameraResourceIndex(uint32 cameraID) const
{
	const auto iter = _cameraIndexMap.find(cameraID);
	return iter == _cameraIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
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

JRenderDB::CameraResource* JRenderDB::FindCameraResource(uint32 cameraID)
{
	const uint32 index = FindCameraResourceIndex(cameraID);
	return index == static_cast<uint32>(-1) ? nullptr : &_cameraResources[index].resource;
}

const JRenderDB::CameraResource* JRenderDB::FindCameraResource(uint32 cameraID) const
{
	const uint32 index = FindCameraResourceIndex(cameraID);
	return index == static_cast<uint32>(-1) ? nullptr : &_cameraResources[index].resource;
}

JMeshResource* JRenderDB::FindMeshResource(const JMesh* mesh)
{
	const auto iter = _meshResources.find(mesh);
	return iter == _meshResources.end() ? nullptr : &iter->second;
}

const JMeshResource* JRenderDB::FindMeshResource(const JMesh* mesh) const
{
	const auto iter = _meshResources.find(mesh);
	return iter == _meshResources.end() ? nullptr : &iter->second;
}

void JRenderDB::SyncMaterial(const JMaterial& material)
{
	JMaterialResource& resource = GetOrCreateMaterialResource(material.instanceID);
	resource.ClearBindings();
	resource.SetShader(material.GetShader());
	resource.SetPipeline(material.GetPipeline());

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

void JRenderDB::SyncCamera(uint32 cameraID, const XMMATRIX& viewProjection, Render::JConstantBuffer* perFrameBuffer)
{
	CameraResource& resource = GetOrCreateCameraResource(cameraID);
	resource.cameraID = cameraID;
	resource.perFrameBuffer = perFrameBuffer;
	XMStoreFloat4x4(&resource.viewProjection, XMMatrixTranspose(viewProjection));
}

JMeshResource* JRenderDB::CreateOrUpdateMeshResource(const JMesh* mesh)
{
	if (_renderContext == nullptr || mesh == nullptr)
	{
		return nullptr;
	}

	RemoveMeshResource(mesh);

	JMeshResource resource;
	Render::JVertexBuffer* vertexBuffer = _renderContext->CreateVertexBuffer(mesh->GetPositions(), mesh->GetVertexCount());
	Render::JIndexBuffer* indexBuffer = _renderContext->CreateIndexBuffer(mesh->GetIndices());
	if (vertexBuffer == nullptr || indexBuffer == nullptr)
	{
		delete vertexBuffer;
		delete indexBuffer;
		return nullptr;
	}

	resource.vertexBuffers.push_back(vertexBuffer);
	resource.indexBufferResource = indexBuffer;
	resource.soaBuffers.push_back(vertexBuffer->view);
	resource.indexBuffer = indexBuffer->view;
	resource.indexSize = mesh->GetIndices().size();

	auto result = _meshResources.emplace(mesh, std::move(resource));
	return &result.first->second;
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

void JRenderDB::RemoveCameraResource(uint32 cameraID)
{
	const uint32 index = FindCameraResourceIndex(cameraID);
	if (index == static_cast<uint32>(-1))
	{
		return;
	}

	const uint32 lastIndex = static_cast<uint32>(_cameraResources.size() - 1);
	if (index != lastIndex)
	{
		_cameraResources[index] = _cameraResources[lastIndex];
		_cameraIndexMap[_cameraResources[index].cameraID] = index;
	}

	_cameraResources.pop_back();
	_cameraIndexMap.erase(cameraID);
}

void JRenderDB::RemoveMeshResource(const JMesh* mesh)
{
	const auto iter = _meshResources.find(mesh);
	if (iter == _meshResources.end())
	{
		return;
	}

	for (Render::JVertexBuffer* vertexBuffer : iter->second.vertexBuffers)
	{
		delete vertexBuffer;
	}
	delete iter->second.indexBufferResource;
	_meshResources.erase(iter);
}

void JRenderDB::Clear()
{
	for (auto& iter : _meshResources)
	{
		for (Render::JVertexBuffer* vertexBuffer : iter.second.vertexBuffers)
		{
			delete vertexBuffer;
		}
		delete iter.second.indexBufferResource;
	}
	_meshResources.clear();
	_materialResources.clear();
	_materialIndexMap.clear();
	_cameraResources.clear();
	_cameraIndexMap.clear();
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
