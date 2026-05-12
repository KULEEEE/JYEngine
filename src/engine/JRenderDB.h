#pragma once
#ifndef __J_RENDER_DB_H__
#define __J_RENDER_DB_H__

#include "engine/precompile.h"
#include "engine/JMaterialResource.h"
#include "engine/JRenderResource.h"
#include "engine/JScene.h"

/*#include "engine/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }
/*#include "engine/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }
/*#include "engine/JRenderContext.h"*/ namespace J { namespace Render { class JRenderContext; } }
/*#include "engine/JGraphicResource.h"*/ namespace J { namespace Render { class JGraphicResource; class JShader; } }

J_ENGINE_BEGIN

class JRenderDB
{
public:
	struct MaterialResourceRecord
	{
		uint32 materialID = 0;
		JMaterialResource resource;
	};

	struct CameraResource
	{
		JCameraHandle camera = {};
		XMFLOAT4X4 viewProjection = {};
		Render::JConstantBuffer* perFrameBuffer = nullptr;
	};

	struct CameraResourceRecord
	{
		uint64 cameraKey = 0;
		CameraResource resource;
	};

	JRenderDB() = default;
	~JRenderDB();

	void Initialize(Render::JRenderContext* renderContext);

	JMaterialResource* FindMaterialResource(uint32 materialID);
	const JMaterialResource* FindMaterialResource(uint32 materialID) const;
	CameraResource* FindCameraResource(JCameraHandle camera);
	const CameraResource* FindCameraResource(JCameraHandle camera) const;
	JMeshResource* FindMeshResource(const JMesh* mesh);
	const JMeshResource* FindMeshResource(const JMesh* mesh) const;

	void SyncMaterial(const JMaterial& material);
	void SyncCamera(JCameraHandle camera, const XMMATRIX& viewProjection, Render::JConstantBuffer* perFrameBuffer);
	JMeshResource* GetOrCreateMeshResource(const JMesh* mesh);
	void RemoveMaterialResource(uint32 materialID);
	void RemoveCameraResource(JCameraHandle camera);
	void RemoveMeshResource(const JMesh* mesh);
	void Clear();

	bool BuildGraphicResource(uint32 materialID, Render::JShader* shader, Render::JGraphicResource& outResource) const;

private:
	JMaterialResource& GetOrCreateMaterialResource(uint32 materialID);
	CameraResource& GetOrCreateCameraResource(JCameraHandle camera);
	uint32 FindMaterialResourceIndex(uint32 materialID) const;
	uint32 FindCameraResourceIndex(JCameraHandle camera) const;
	static uint64 MakeCameraKey(JCameraHandle camera);

	Render::JRenderContext* _renderContext = nullptr;
	std::vector<MaterialResourceRecord> _materialResources;
	std::unordered_map<uint32, uint32> _materialIndexMap;
	std::vector<CameraResourceRecord> _cameraResources;
	std::unordered_map<uint64, uint32> _cameraIndexMap;
	std::unordered_map<const JMesh*, JMeshResource> _meshResources;
};

J_ENGINE_END

#endif
