#pragma once
#ifndef __J_RENDER_DB_H__
#define __J_RENDER_DB_H__

#include "engine/precompile.h"
#include "engine/render/JMaterialResource.h"
#include "engine/render/JRenderResource.h"
#include "engine/scene/JScene.h"

/*#include "engine/asset/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }
/*#include "engine/asset/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }
/*#include "engine/render/JRenderContext.h"*/ namespace J { namespace Render { class JRenderContext; } }
/*#include "engine/render/JGraphicResource.h"*/ namespace J { namespace Render { class JGraphicResource; class JShader; } }

J_ENGINE_BEGIN

class JRenderDB
{
public:
	struct MaterialResourceRecord
	{
		uint32 materialID = 0;
		JMaterialResource resource;
	};

	struct CameraResourceRecord
	{
		uint64 cameraKey = 0;
		JCameraResource resource;
	};

	struct TransformResourceRecord
	{
		uint64 transformKey = 0;
		JTransformResource resource;
	};

	JRenderDB() = default;
	~JRenderDB();

	void Initialize(Render::JRenderContext* renderContext);

	JMaterialResource* FindMaterialResource(uint32 materialID);
	const JMaterialResource* FindMaterialResource(uint32 materialID) const;
	JCameraResource* FindCameraResource(JCameraHandle camera);
	const JCameraResource* FindCameraResource(JCameraHandle camera) const;
	JTransformResource* FindTransformResource(JTransformHandle transform);
	const JTransformResource* FindTransformResource(JTransformHandle transform) const;
	JMeshResource* FindMeshResource(const JMesh* mesh);
	const JMeshResource* FindMeshResource(const JMesh* mesh) const;

	void SyncMaterial(const JMaterial& material);
	void SyncCamera(JCameraHandle camera, const XMMATRIX& viewProjection, Render::JConstantBuffer* perFrameBuffer);
	void SyncTransform(JTransformHandle transform, const XMMATRIX& world);
	JMeshResource* GetOrCreateMeshResource(const JMesh* mesh);
	void RemoveMaterialResource(uint32 materialID);
	void RemoveCameraResource(JCameraHandle camera);
	void RemoveTransformResource(JTransformHandle transform);
	void RemoveMeshResource(const JMesh* mesh);
	void Clear();

	bool BuildGraphicResource(uint32 materialID, Render::JShader* shader, Render::JGraphicResource& outResource) const;

private:
	JMaterialResource& GetOrCreateMaterialResource(uint32 materialID);
	JCameraResource& GetOrCreateCameraResource(JCameraHandle camera);
	JTransformResource& GetOrCreateTransformResource(JTransformHandle transform);
	uint32 FindMaterialResourceIndex(uint32 materialID) const;
	uint32 FindCameraResourceIndex(JCameraHandle camera) const;
	uint32 FindTransformResourceIndex(JTransformHandle transform) const;
	static uint64 MakeCameraKey(JCameraHandle camera);
	static uint64 MakeTransformKey(JTransformHandle transform);

	Render::JRenderContext* _renderContext = nullptr;
	std::vector<MaterialResourceRecord> _materialResources;
	std::unordered_map<uint32, uint32> _materialIndexMap;
	std::vector<CameraResourceRecord> _cameraResources;
	std::unordered_map<uint64, uint32> _cameraIndexMap;
	std::vector<TransformResourceRecord> _transformResources;
	std::unordered_map<uint64, uint32> _transformIndexMap;
	std::unordered_map<const JMesh*, JMeshResource> _meshResources;
};

J_ENGINE_END

#endif
