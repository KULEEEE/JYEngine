#pragma once
#ifndef __J_RENDER_DB_H__
#define __J_RENDER_DB_H__

#include "engine/precompile.h"
#include "engine/render/JRenderResource.h"
#include "engine/render/JRenderSnapshot.h"
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
	JLightResource* FindLightResource();
	const JLightResource* FindLightResource() const;
	JMeshResource* FindMeshResource(const JMesh* mesh);
	const JMeshResource* FindMeshResource(const JMesh* mesh) const;

	void SyncMaterial(const JMaterial& material);
	void SyncCamera(JCameraHandle camera, const XMMATRIX& viewProjection);
	void SyncTransform(JTransformHandle transform, const XMMATRIX& world);
	void SyncLight(const JLightSnapshot& snapshot);
	JMeshResource* GetOrCreateMeshResource(const JMesh* mesh);
	void RemoveMaterialResource(uint32 materialID);
	void RemoveCameraResource(JCameraHandle camera);
	void RemoveTransformResource(JTransformHandle transform);
	void RemoveMeshResource(const JMesh* mesh);
	void PruneUnusedSceneResources(
		const std::unordered_set<uint64>& activeCameraKeys,
		const std::unordered_set<uint64>& activeTransformKeys,
		const std::unordered_set<const JMesh*>& activeMeshes);
	void Clear();

	bool BuildGraphicResource(uint32 materialID, Render::JShader* shader, Render::JGraphicResource& outResource) const;

private:
	JMaterialResource& getOrCreateMaterialResource(uint32 materialID);
	JCameraResource& getOrCreateCameraResource(JCameraHandle camera);
	JTransformResource& getOrCreateTransformResource(JTransformHandle transform);
	JLightResource& getOrCreateLightResource();
	uint32 findMaterialResourceIndex(uint32 materialID) const;
	uint32 findCameraResourceIndex(JCameraHandle camera) const;
	uint32 findTransformResourceIndex(JTransformHandle transform) const;
	static uint64 makeCameraKey(JCameraHandle camera);
	static uint64 makeTransformKey(JTransformHandle transform);

	Render::JRenderContext* _renderContext = nullptr;
	std::vector<MaterialResourceRecord> _materialResources;
	std::unordered_map<uint32, uint32> _materialIndexMap;
	std::vector<CameraResourceRecord> _cameraResources;
	std::unordered_map<uint64, uint32> _cameraIndexMap;
	std::vector<TransformResourceRecord> _transformResources;
	std::unordered_map<uint64, uint32> _transformIndexMap;
	JLightResource _lightResource;
	std::unordered_map<const JMesh*, JMeshResource> _meshResources;
};

J_ENGINE_END

#endif
