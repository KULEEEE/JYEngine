#pragma once
#ifndef __J_RENDER_DB_H__
#define __J_RENDER_DB_H__

#include "engine/precompile.h"
#include "engine/JMaterialResource.h"
#include "engine/JRenderResource.h"

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
		uint32 cameraID = 0;
		XMFLOAT4X4 viewProjection = {};
		Render::JConstantBuffer* perFrameBuffer = nullptr;
	};

	struct CameraResourceRecord
	{
		uint32 cameraID = 0;
		CameraResource resource;
	};

	JRenderDB() = default;
	~JRenderDB();

	void Initialize(Render::JRenderContext* renderContext);

	JMaterialResource* FindMaterialResource(uint32 materialID);
	const JMaterialResource* FindMaterialResource(uint32 materialID) const;
	CameraResource* FindCameraResource(uint32 cameraID);
	const CameraResource* FindCameraResource(uint32 cameraID) const;
	JMeshResource* FindMeshResource(const JMesh* mesh);
	const JMeshResource* FindMeshResource(const JMesh* mesh) const;

	void SyncMaterial(const JMaterial& material);
	void SyncCamera(uint32 cameraID, const XMMATRIX& viewProjection, Render::JConstantBuffer* perFrameBuffer);
	JMeshResource* CreateOrUpdateMeshResource(const JMesh* mesh);
	void RemoveMaterialResource(uint32 materialID);
	void RemoveCameraResource(uint32 cameraID);
	void RemoveMeshResource(const JMesh* mesh);
	void Clear();

	bool BuildGraphicResource(uint32 materialID, Render::JShader* shader, Render::JGraphicResource& outResource) const;

private:
	JMaterialResource& GetOrCreateMaterialResource(uint32 materialID);
	CameraResource& GetOrCreateCameraResource(uint32 cameraID);
	uint32 FindMaterialResourceIndex(uint32 materialID) const;
	uint32 FindCameraResourceIndex(uint32 cameraID) const;

	Render::JRenderContext* _renderContext = nullptr;
	std::vector<MaterialResourceRecord> _materialResources;
	std::unordered_map<uint32, uint32> _materialIndexMap;
	std::vector<CameraResourceRecord> _cameraResources;
	std::unordered_map<uint32, uint32> _cameraIndexMap;
	std::unordered_map<const JMesh*, JMeshResource> _meshResources;
};

J_ENGINE_END

#endif
