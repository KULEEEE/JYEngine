#include "engine/render/JRenderDB.h"

#include "engine/render/JGraphicResource.h"
#include "engine/render/JRenderContext.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"
#include "engine/render/JMaterialResource.h"

J_ENGINE_BEGIN

namespace
{
	struct PerObjectConstants
	{
		XMFLOAT4X4 world;
	};
}

JRenderDB::~JRenderDB()
{
	Clear();
}

uint64 JRenderDB::MakeCameraKey(JCameraHandle camera)
{
	return (static_cast<uint64>(camera.generation) << 32) | camera.index;
}

uint64 JRenderDB::MakeTransformKey(JTransformHandle transform)
{
	return (static_cast<uint64>(transform.generation) << 32) | transform.index;
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

JCameraResource& JRenderDB::GetOrCreateCameraResource(JCameraHandle camera)
{
	const uint64 cameraKey = MakeCameraKey(camera);
	const auto iter = _cameraIndexMap.find(cameraKey);
	if (iter != _cameraIndexMap.end())
	{
		return _cameraResources[iter->second].resource;
	}

	CameraResourceRecord record;
	record.cameraKey = cameraKey;
	record.resource.camera = camera;

	const uint32 newIndex = static_cast<uint32>(_cameraResources.size());
	_cameraResources.push_back(record);
	_cameraIndexMap[cameraKey] = newIndex;
	return _cameraResources.back().resource;
}

uint32 JRenderDB::FindCameraResourceIndex(JCameraHandle camera) const
{
	const auto iter = _cameraIndexMap.find(MakeCameraKey(camera));
	return iter == _cameraIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
}

JTransformResource& JRenderDB::GetOrCreateTransformResource(JTransformHandle transform)
{
	const uint64 transformKey = MakeTransformKey(transform);
	const auto iter = _transformIndexMap.find(transformKey);
	if (iter != _transformIndexMap.end())
	{
		return _transformResources[iter->second].resource;
	}

	TransformResourceRecord record;
	record.transformKey = transformKey;
	record.resource.transform = transform;

	const uint32 newIndex = static_cast<uint32>(_transformResources.size());
	_transformResources.push_back(record);
	_transformIndexMap[transformKey] = newIndex;
	return _transformResources.back().resource;
}

uint32 JRenderDB::FindTransformResourceIndex(JTransformHandle transform) const
{
	const auto iter = _transformIndexMap.find(MakeTransformKey(transform));
	return iter == _transformIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
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

JCameraResource* JRenderDB::FindCameraResource(JCameraHandle camera)
{
	const uint32 index = FindCameraResourceIndex(camera);
	return index == static_cast<uint32>(-1) ? nullptr : &_cameraResources[index].resource;
}

const JCameraResource* JRenderDB::FindCameraResource(JCameraHandle camera) const
{
	const uint32 index = FindCameraResourceIndex(camera);
	return index == static_cast<uint32>(-1) ? nullptr : &_cameraResources[index].resource;
}

JTransformResource* JRenderDB::FindTransformResource(JTransformHandle transform)
{
	const uint32 index = FindTransformResourceIndex(transform);
	return index == static_cast<uint32>(-1) ? nullptr : &_transformResources[index].resource;
}

const JTransformResource* JRenderDB::FindTransformResource(JTransformHandle transform) const
{
	const uint32 index = FindTransformResourceIndex(transform);
	return index == static_cast<uint32>(-1) ? nullptr : &_transformResources[index].resource;
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

void JRenderDB::SyncCamera(JCameraHandle camera, const XMMATRIX& viewProjection, Render::JConstantBuffer* perFrameBuffer)
{
	JCameraResource& resource = GetOrCreateCameraResource(camera);
	resource.camera = camera;
	resource.perFrameBuffer = perFrameBuffer;
	XMStoreFloat4x4(&resource.viewProjection, XMMatrixTranspose(viewProjection));
}

void JRenderDB::SyncTransform(JTransformHandle transform, const XMMATRIX& world)
{
	if (_renderContext == nullptr || !transform.IsValid())
	{
		return;
	}

	JTransformResource& resource = GetOrCreateTransformResource(transform);
	resource.transform = transform;
	XMStoreFloat4x4(&resource.world, XMMatrixTranspose(world));

	PerObjectConstants constants{};
	constants.world = resource.world;
	if (resource.perObjectBuffer == nullptr)
	{
		resource.perObjectBuffer = _renderContext->CreateConstantBuffer(&constants, sizeof(constants));
		return;
	}

	_renderContext->UpdateConstantBuffer(resource.perObjectBuffer, &constants, sizeof(constants));
}

JMeshResource* JRenderDB::GetOrCreateMeshResource(const JMesh* mesh)
{
	if (_renderContext == nullptr || mesh == nullptr)
	{
		return nullptr;
	}

	JMeshResource* existingResource = FindMeshResource(mesh);
	if (existingResource != nullptr)
	{
		return existingResource;
	}

	JMeshResource resource;
	Render::JVertexBuffer* vertexBuffer = _renderContext->CreateVertexBuffer(mesh->GetPositions(), mesh->GetVertexCount());
	Render::JVertexBuffer* normalBuffer = nullptr;
	Render::JVertexBuffer* texcoordBuffer = nullptr;
	if (mesh->GetNormals().size() == mesh->GetVertexCount() * 3)
	{
		normalBuffer = _renderContext->CreateVertexBuffer(mesh->GetNormals(), mesh->GetVertexCount());
	}
	if (mesh->GetTexcoords(0).size() == mesh->GetVertexCount() * 2)
	{
		texcoordBuffer = _renderContext->CreateVertexBuffer(mesh->GetTexcoords(0), mesh->GetVertexCount());
	}
	Render::JIndexBuffer* indexBuffer = _renderContext->CreateIndexBuffer(mesh->GetIndices());
	if (vertexBuffer == nullptr || indexBuffer == nullptr)
	{
		delete vertexBuffer;
		delete normalBuffer;
		delete texcoordBuffer;
		delete indexBuffer;
		return nullptr;
	}

	resource.vertexBuffers.push_back(vertexBuffer);
	resource.indexBufferResource = indexBuffer;
	resource.soaBuffers.push_back(vertexBuffer->view);
	if (normalBuffer != nullptr)
	{
		resource.vertexBuffers.push_back(normalBuffer);
		resource.soaBuffers.push_back(normalBuffer->view);
		resource.hasNormals = true;
	}
	if (texcoordBuffer != nullptr)
	{
		resource.vertexBuffers.push_back(texcoordBuffer);
		resource.soaBuffers.push_back(texcoordBuffer->view);
		resource.hasTexcoords = true;
	}
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

void JRenderDB::RemoveCameraResource(JCameraHandle camera)
{
	const uint32 index = FindCameraResourceIndex(camera);
	if (index == static_cast<uint32>(-1))
	{
		return;
	}

	const uint32 lastIndex = static_cast<uint32>(_cameraResources.size() - 1);
	if (index != lastIndex)
	{
		_cameraResources[index] = _cameraResources[lastIndex];
		_cameraIndexMap[_cameraResources[index].cameraKey] = index;
	}

	_cameraResources.pop_back();
	_cameraIndexMap.erase(MakeCameraKey(camera));
}

void JRenderDB::RemoveTransformResource(JTransformHandle transform)
{
	const uint32 index = FindTransformResourceIndex(transform);
	if (index == static_cast<uint32>(-1))
	{
		return;
	}

	delete _transformResources[index].resource.perObjectBuffer;

	const uint32 lastIndex = static_cast<uint32>(_transformResources.size() - 1);
	if (index != lastIndex)
	{
		_transformResources[index] = _transformResources[lastIndex];
		_transformIndexMap[_transformResources[index].transformKey] = index;
	}

	_transformResources.pop_back();
	_transformIndexMap.erase(MakeTransformKey(transform));
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

	for (TransformResourceRecord& record : _transformResources)
	{
		delete record.resource.perObjectBuffer;
	}

	_meshResources.clear();
	_materialResources.clear();
	_materialIndexMap.clear();
	_cameraResources.clear();
	_cameraIndexMap.clear();
	_transformResources.clear();
	_transformIndexMap.clear();
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
