#include "engine/render/JRenderDB.h"

#include "engine/render/JGraphicResource.h"
#include "engine/render/JRenderContext.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"

J_ENGINE_BEGIN

namespace
{
	constexpr uint32 MAX_RENDER_LIGHTS = 8;

	struct PerObjectConstants
	{
		XMFLOAT4X4 world;
	};

	struct PerFrameConstants
	{
		XMFLOAT4X4 viewProjection;
	};

	struct LightConstants
	{
		JVec4 colorIntensities[MAX_RENDER_LIGHTS];
		JVec4 positions[MAX_RENDER_LIGHTS];
		JVec4 info;
	};

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
	record.resource.materialID = materialID;

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

JLightResource& JRenderDB::GetOrCreateLightResource()
{
	return _lightResource;
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
	resource.shader = material.GetShader();
	resource.pipeline = material.GetPipeline();
	resource.constantBuffers.clear();
	resource.textures.clear();

	for (const JMaterial::ConstantBufferParam& param : material.GetConstantBuffers())
	{
		resource.constantBuffers.push_back({ param.name, param.nameHash, param.buffer });
	}

	for (const JMaterial::TextureParam& param : material.GetTextures())
	{
		resource.textures.push_back({ param.name, param.nameHash, param.texture });
	}
}

void JRenderDB::SyncCamera(JCameraHandle camera, const XMMATRIX& viewProjection)
{
	if (_renderContext == nullptr || !camera.IsValid())
	{
		return;
	}

	JCameraResource& resource = GetOrCreateCameraResource(camera);
	resource.camera = camera;

	PerFrameConstants constants{};
	XMStoreFloat4x4(&constants.viewProjection, XMMatrixTranspose(viewProjection));
	if (resource.perFrameBuffer == nullptr)
	{
		resource.perFrameBuffer = _renderContext->CreateConstantBuffer(&constants, sizeof(constants));
		return;
	}

	_renderContext->UpdateConstantBuffer(resource.perFrameBuffer, &constants, sizeof(constants));
}

void JRenderDB::SyncTransform(JTransformHandle transform, const XMMATRIX& world)
{
	if (_renderContext == nullptr || !transform.IsValid())
	{
		return;
	}

	JTransformResource& resource = GetOrCreateTransformResource(transform);
	resource.transform = transform;

	PerObjectConstants constants{};
	XMStoreFloat4x4(&constants.world, XMMatrixTranspose(world));
	if (resource.perObjectBuffer == nullptr)
	{
		resource.perObjectBuffer = _renderContext->CreateConstantBuffer(&constants, sizeof(constants));
		return;
	}

	_renderContext->UpdateConstantBuffer(resource.perObjectBuffer, &constants, sizeof(constants));
}

void JRenderDB::SyncLight(const JLightSnapshot& snapshot)
{
	if (_renderContext == nullptr)
	{
		return;
	}

	JLightResource& resource = GetOrCreateLightResource();
	resource.lightCount = static_cast<uint32>(snapshot.items.size() < MAX_RENDER_LIGHTS ? snapshot.items.size() : MAX_RENDER_LIGHTS);

	LightConstants constants{};
	for (uint32 i = 0; i < resource.lightCount; ++i)
	{
		const JLightSnapshotItem& item = snapshot.items[i];
		constants.colorIntensities[i] = item.colorIntensity;
		constants.positions[i] = item.position;
	}
	constants.info = JVec4(static_cast<float>(resource.lightCount), 0.0f, 0.0f, 0.0f);

	if (resource.lightBuffer == nullptr)
	{
		resource.lightBuffer = _renderContext->CreateConstantBuffer(&constants, sizeof(constants));
		return;
	}

	_renderContext->UpdateConstantBuffer(resource.lightBuffer, &constants, sizeof(constants));
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

	destroyConstantBuffer(_renderContext, _cameraResources[index].resource.perFrameBuffer);

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

	destroyConstantBuffer(_renderContext, _transformResources[index].resource.perObjectBuffer);

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

	destroyMeshResource(_renderContext, iter->second);
	_meshResources.erase(iter);
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

	for (auto iter = _meshResources.begin(); iter != _meshResources.end();)
	{
		if (activeMeshes.find(iter->first) != activeMeshes.end())
		{
			++iter;
			continue;
		}

		destroyMeshResource(_renderContext, iter->second);
		iter = _meshResources.erase(iter);
	}
}

void JRenderDB::Clear()
{
	for (auto& iter : _meshResources)
	{
		destroyMeshResource(_renderContext, iter.second);
	}

	for (TransformResourceRecord& record : _transformResources)
	{
		destroyConstantBuffer(_renderContext, record.resource.perObjectBuffer);
	}
	destroyConstantBuffer(_renderContext, _lightResource.lightBuffer);

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
