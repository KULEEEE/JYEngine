#include "engine/render/JRenderDB.h"

#include "engine/render/JGraphicResource.h"
#include "engine/render/JRenderContext.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"

J_ENGINE_BEGIN

namespace
{
	constexpr uint32 MAX_RENDER_LIGHTS = 8;

	void destroyConstantBuffer(Render::JRenderContext* renderContext, Render::JConstantBuffer*& buffer)
	{
		if (buffer == nullptr)
		{
			return;
		}

		if (renderContext != nullptr)
		{
			renderContext->DestroyConstantBuffer(buffer);
		}
		else
		{
			buffer->Destroy();
			delete buffer;
		}
		buffer = nullptr;
	}

	void destroyMeshResource(Render::JRenderContext* renderContext, JMeshResource& resource)
	{
		for (Render::JVertexBuffer* vertexBuffer : resource.vertexBuffers)
		{
			if (renderContext != nullptr)
			{
				renderContext->DestroyVertexBuffer(vertexBuffer);
			}
			else if (vertexBuffer != nullptr)
			{
				vertexBuffer->Destroy();
				delete vertexBuffer;
			}
		}

		if (resource.indexBufferResource != nullptr)
		{
			if (renderContext != nullptr)
			{
				renderContext->DestroyIndexBuffer(resource.indexBufferResource);
			}
			else
			{
				resource.indexBufferResource->Destroy();
				delete resource.indexBufferResource;
			}
			resource.indexBufferResource = nullptr;
		}

		resource.vertexBuffers.clear();
		resource.soaBuffers.clear();
		resource.indexSize = 0;
		resource.hasNormals = false;
		resource.hasTexcoords = false;
	}

	void destroyMaterialResource(Render::JRenderContext* renderContext, JMaterialResource& resource)
	{
		for (JMaterialResource::ConstantBufferEntry& entry : resource.constantBuffers)
		{
			destroyConstantBuffer(renderContext, entry.buffer);
		}

		for (JMaterialResource::TextureEntry& entry : resource.textures)
		{
			if (entry.texture == nullptr)
			{
				continue;
			}

			if (renderContext != nullptr)
			{
				renderContext->DestroyTexture(entry.texture);
			}
			else
			{
				entry.texture->Destroy();
				delete entry.texture;
			}
			entry.texture = nullptr;
		}

		if (resource.pipeline != nullptr)
		{
			if (renderContext != nullptr)
			{
				renderContext->DestroyPipeline(resource.pipeline);
			}
			else
			{
				delete resource.pipeline;
			}
			resource.pipeline = nullptr;
		}

		if (resource.shader != nullptr)
		{
			if (renderContext != nullptr)
			{
				renderContext->DestroyShader(resource.shader);
			}
			else
			{
				delete resource.shader;
			}
			resource.shader = nullptr;
		}

		resource.constantBuffers.clear();
		resource.textures.clear();
	}
}

JRenderDB::~JRenderDB()
{
	Clear();
}

uint64 JRenderDB::makeCameraKey(JCameraHandle camera)
{
	return (static_cast<uint64>(camera.generation) << 32) | camera.index;
}

uint64 JRenderDB::makeTransformKey(JTransformHandle transform)
{
	return (static_cast<uint64>(transform.generation) << 32) | transform.index;
}

void JRenderDB::Initialize(Render::JRenderContext* renderContext)
{
	_renderContext = renderContext;
}

JMaterialResource& JRenderDB::getOrCreateMaterialResource(uint32 materialID)
{
	const auto iter = _materialIndexMap.find(materialID);
	if (iter != _materialIndexMap.end())
	{
		return _materialResources[iter->second].resource;
	}

	MaterialResourceRecord record;
	record.materialID = materialID;
	record.resource.materialID = materialID;

	const uint32 newIndex = static_cast<uint32>(_materialResources.size());
	_materialResources.push_back(record);
	_materialIndexMap[materialID] = newIndex;
	return _materialResources.back().resource;
}

uint32 JRenderDB::findMaterialResourceIndex(uint32 materialID) const
{
	const auto iter = _materialIndexMap.find(materialID);
	return iter == _materialIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
}

JCameraResource& JRenderDB::getOrCreateCameraResource(JCameraHandle camera)
{
	const uint64 cameraKey = makeCameraKey(camera);
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

uint32 JRenderDB::findCameraResourceIndex(JCameraHandle camera) const
{
	const auto iter = _cameraIndexMap.find(makeCameraKey(camera));
	return iter == _cameraIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
}

JTransformResource& JRenderDB::getOrCreateTransformResource(JTransformHandle transform)
{
	const uint64 transformKey = makeTransformKey(transform);
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

JLightResource& JRenderDB::getOrCreateLightResource()
{
	return _lightResource;
}

uint32 JRenderDB::findTransformResourceIndex(JTransformHandle transform) const
{
	const auto iter = _transformIndexMap.find(makeTransformKey(transform));
	return iter == _transformIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
}

uint32 JRenderDB::findMeshResourceIndex(const JMesh* mesh) const
{
	const auto iter = _meshIndexMap.find(mesh);
	return iter == _meshIndexMap.end() ? static_cast<uint32>(-1) : iter->second;
}

JMaterialResource* JRenderDB::FindMaterialResource(uint32 materialID)
{
	const uint32 index = findMaterialResourceIndex(materialID);
	return index == static_cast<uint32>(-1) ? nullptr : &_materialResources[index].resource;
}

const JMaterialResource* JRenderDB::FindMaterialResource(uint32 materialID) const
{
	const uint32 index = findMaterialResourceIndex(materialID);
	return index == static_cast<uint32>(-1) ? nullptr : &_materialResources[index].resource;
}

JCameraResource* JRenderDB::FindCameraResource(JCameraHandle camera)
{
	const uint32 index = findCameraResourceIndex(camera);
	return index == static_cast<uint32>(-1) ? nullptr : &_cameraResources[index].resource;
}

const JCameraResource* JRenderDB::FindCameraResource(JCameraHandle camera) const
{
	const uint32 index = findCameraResourceIndex(camera);
	return index == static_cast<uint32>(-1) ? nullptr : &_cameraResources[index].resource;
}

JTransformResource* JRenderDB::FindTransformResource(JTransformHandle transform)
{
	const uint32 index = findTransformResourceIndex(transform);
	return index == static_cast<uint32>(-1) ? nullptr : &_transformResources[index].resource;
}

const JTransformResource* JRenderDB::FindTransformResource(JTransformHandle transform) const
{
	const uint32 index = findTransformResourceIndex(transform);
	return index == static_cast<uint32>(-1) ? nullptr : &_transformResources[index].resource;
}

JLightResource* JRenderDB::FindLightResource()
{
	return &_lightResource;
}

const JLightResource* JRenderDB::FindLightResource() const
{
	return &_lightResource;
}

JMeshResource* JRenderDB::FindMeshResource(const JMesh* mesh)
{
	const uint32 index = findMeshResourceIndex(mesh);
	return index == static_cast<uint32>(-1) ? nullptr : &_meshResources[index].resource;
}

const JMeshResource* JRenderDB::FindMeshResource(const JMesh* mesh) const
{
	const uint32 index = findMeshResourceIndex(mesh);
	return index == static_cast<uint32>(-1) ? nullptr : &_meshResources[index].resource;
}

const JMaterialResource* JRenderDB::GetMaterialResourceByIndex(uint32 index) const
{
	return index < _materialResources.size() ? &_materialResources[index].resource : nullptr;
}

const JTransformResource* JRenderDB::GetTransformResourceByIndex(uint32 index) const
{
	return index < _transformResources.size() ? &_transformResources[index].resource : nullptr;
}

const JMeshResource* JRenderDB::GetMeshResourceByIndex(uint32 index) const
{
	return index < _meshResources.size() ? &_meshResources[index].resource : nullptr;
}

uint32 JRenderDB::GetMaterialResourceIndex(uint32 materialID) const
{
	return findMaterialResourceIndex(materialID);
}

uint32 JRenderDB::GetTransformResourceIndex(JTransformHandle transform) const
{
	return findTransformResourceIndex(transform);
}

uint32 JRenderDB::GetMeshResourceIndex(const JMesh* mesh) const
{
	return findMeshResourceIndex(mesh);
}

void JRenderDB::SyncMaterial(const JMaterial& material)
{
	if (_renderContext == nullptr)
	{
		return;
	}

	JMaterialResource& resource = getOrCreateMaterialResource(material.instanceID);
	destroyMaterialResource(_renderContext, resource);
	resource.materialID = material.instanceID;
	resource.shader = _renderContext->CreateShader(material.GetShaderPath());
	if (resource.shader == nullptr)
	{
		return;
	}

	resource.pipeline = _renderContext->CreatePipeline(resource.shader, material.IsAlphaBlendEnabled());
	if (resource.pipeline == nullptr)
	{
		destroyMaterialResource(_renderContext, resource);
		return;
	}

	for (const JMaterial::ConstantBufferParam& param : material.GetConstantBuffers())
	{
		if (param.data.empty())
		{
			continue;
		}

		Render::JConstantBuffer* buffer = _renderContext->CreateConstantBuffer(
			const_cast<uint8*>(param.data.data()),
			param.data.size());
		if (buffer != nullptr)
		{
			resource.constantBuffers.push_back({ param.name, param.nameHash, buffer });
		}
	}

	for (const JMaterial::TextureParam& param : material.GetTextures())
	{
		if (param.path.empty())
		{
			continue;
		}

		Render::JTexture* texture = _renderContext->CreateTextureFromFile(param.path);
		if (texture != nullptr)
		{
			resource.textures.push_back({ param.name, param.nameHash, texture });
		}
	}
}

void JRenderDB::SyncCamera(JCameraHandle camera, const XMMATRIX& viewProjection)
{
	if (_renderContext == nullptr || !camera.IsValid())
	{
		return;
	}

	JCameraResource& resource = getOrCreateCameraResource(camera);
	resource.camera = camera;

	XMStoreFloat4x4(&resource.constants.viewProjection, XMMatrixTranspose(viewProjection));
}

void JRenderDB::SyncTransform(JTransformHandle transform, const XMMATRIX& world)
{
	if (_renderContext == nullptr || !transform.IsValid())
	{
		return;
	}

	JTransformResource& resource = getOrCreateTransformResource(transform);
	resource.transform = transform;
	resource.world = world;

	XMStoreFloat4x4(&resource.constants.world, XMMatrixTranspose(world));
}

void JRenderDB::SyncLight(const JLightSnapshot& snapshot)
{
	if (_renderContext == nullptr)
	{
		return;
	}

	JLightResource& resource = getOrCreateLightResource();
	resource.lightCount = static_cast<uint32>(snapshot.items.size() < MAX_RENDER_LIGHTS ? snapshot.items.size() : MAX_RENDER_LIGHTS);

	resource.constants = {};
	for (uint32 i = 0; i < resource.lightCount; ++i)
	{
		const JLightSnapshot::Item& item = snapshot.items[i];
		resource.constants.colorIntensities[i] = item.colorIntensity;
		resource.constants.positions[i] = item.position;
	}
	resource.constants.info = JVec4(static_cast<float>(resource.lightCount), 0.0f, 0.0f, 0.0f);
	resource.initialized = true;
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
		_renderContext->DestroyVertexBuffer(vertexBuffer);
		_renderContext->DestroyVertexBuffer(normalBuffer);
		_renderContext->DestroyVertexBuffer(texcoordBuffer);
		_renderContext->DestroyIndexBuffer(indexBuffer);
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

	MeshResourceRecord record;
	record.mesh = mesh;
	record.resource = std::move(resource);

	const uint32 newIndex = static_cast<uint32>(_meshResources.size());
	_meshResources.push_back(std::move(record));
	_meshIndexMap[mesh] = newIndex;
	return &_meshResources.back().resource;
}

void JRenderDB::RemoveMaterialResource(uint32 materialID)
{
	const uint32 index = findMaterialResourceIndex(materialID);
	if (index == static_cast<uint32>(-1))
	{
		return;
	}

	destroyMaterialResource(_renderContext, _materialResources[index].resource);

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
	const uint32 index = findCameraResourceIndex(camera);
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
	_cameraIndexMap.erase(makeCameraKey(camera));
}

void JRenderDB::RemoveTransformResource(JTransformHandle transform)
{
	const uint32 index = findTransformResourceIndex(transform);
	if (index == static_cast<uint32>(-1))
	{
		return;
	}

	const uint32 lastIndex = static_cast<uint32>(_transformResources.size() - 1);
	if (index != lastIndex)
	{
		_transformResources[index] = _transformResources[lastIndex];
		_transformIndexMap[_transformResources[index].transformKey] = index;
	}

	_transformResources.pop_back();
	_transformIndexMap.erase(makeTransformKey(transform));
}

void JRenderDB::RemoveMeshResource(const JMesh* mesh)
{
	const uint32 index = findMeshResourceIndex(mesh);
	if (index == static_cast<uint32>(-1))
	{
		return;
	}

	destroyMeshResource(_renderContext, _meshResources[index].resource);

	const uint32 lastIndex = static_cast<uint32>(_meshResources.size() - 1);
	if (index != lastIndex)
	{
		_meshResources[index] = std::move(_meshResources[lastIndex]);
		_meshIndexMap[_meshResources[index].mesh] = index;
	}

	_meshResources.pop_back();
	_meshIndexMap.erase(mesh);
}

void JRenderDB::PruneUnusedSceneResources(
	const std::unordered_set<uint64>& activeCameraKeys,
	const std::unordered_set<uint64>& activeTransformKeys,
	const std::unordered_set<const JMesh*>& activeMeshes)
{
	for (uint32 index = 0; index < _cameraResources.size();)
	{
		if (activeCameraKeys.find(_cameraResources[index].cameraKey) != activeCameraKeys.end())
		{
			++index;
			continue;
		}

		const JCameraHandle camera = _cameraResources[index].resource.camera;
		RemoveCameraResource(camera);
	}

	for (uint32 index = 0; index < _transformResources.size();)
	{
		if (activeTransformKeys.find(_transformResources[index].transformKey) != activeTransformKeys.end())
		{
			++index;
			continue;
		}

		const JTransformHandle transform = _transformResources[index].resource.transform;
		RemoveTransformResource(transform);
	}

	for (uint32 index = 0; index < _meshResources.size();)
	{
		if (activeMeshes.find(_meshResources[index].mesh) != activeMeshes.end())
		{
			++index;
			continue;
		}

		const JMesh* mesh = _meshResources[index].mesh;
		RemoveMeshResource(mesh);
	}
}

void JRenderDB::Clear()
{
	for (MeshResourceRecord& record : _meshResources)
	{
		destroyMeshResource(_renderContext, record.resource);
	}

	for (MaterialResourceRecord& record : _materialResources)
	{
		destroyMaterialResource(_renderContext, record.resource);
	}

	_meshResources.clear();
	_meshIndexMap.clear();
	_materialResources.clear();
	_materialIndexMap.clear();
	_cameraResources.clear();
	_cameraIndexMap.clear();
	_transformResources.clear();
	_transformIndexMap.clear();
}

JRenderDB::ResolvedDrawResources JRenderDB::ResolveDrawResources(const JDrawItem& drawItem) const
{
	ResolvedDrawResources resources;
	resources.mesh = GetMeshResourceByIndex(drawItem.meshResourceIndex);
	resources.material = GetMaterialResourceByIndex(drawItem.materialResourceIndex);
	resources.transform = GetTransformResourceByIndex(drawItem.transformResourceIndex);
	if (resources.mesh == nullptr)
	{
		resources.mesh = FindMeshResource(drawItem.mesh);
	}
	if (resources.material == nullptr)
	{
		resources.material = FindMaterialResource(drawItem.materialID);
	}
	if (resources.transform == nullptr)
	{
		resources.transform = FindTransformResource(drawItem.transform);
	}
	return resources;
}

bool JRenderDB::BuildGraphicResource(uint32 materialID, Render::JShader* shader, Render::JGraphicResource& outResource) const
{
	const JMaterialResource* materialResource = FindMaterialResource(materialID);
	if (materialResource == nullptr || shader == nullptr)
	{
		return false;
	}

	outResource.SetShader(shader);

	for (const JMaterialResource::ConstantBufferEntry& entry : materialResource->constantBuffers)
	{
		outResource.SetConstantBuffer(entry.nameHash, entry.buffer, entry.name);
	}

	for (const JMaterialResource::TextureEntry& entry : materialResource->textures)
	{
		outResource.SetTexture(entry.nameHash, entry.texture, entry.name);
	}

	return true;
}

J_ENGINE_END
