#pragma once
#ifndef __J_RENDER_SERVER_H__
#define __J_RENDER_SERVER_H__

#include "engine/precompile.h"
#include "engine/JRenderDB.h"

/*#include "engine/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }
/*#include "engine/JCameraComponent.h"*/ namespace J { namespace Engine { class JCameraComponent; } }
/*#include "engine/JGraphicResource.h"*/ namespace J { namespace Render { class JGraphicResource; class JShader; } }
/*#include "engine/JRenderDefinition.h"*/ namespace J { namespace Render { struct JConstantBuffer; } }

J_ENGINE_BEGIN

class JRenderServer
{
public:
	struct MaterialRecord
	{
		uint32 materialID = 0;
		JMaterial* source = nullptr;
	};

	struct CameraRecord
	{
		uint32 cameraID = 0;
		JCameraComponent* source = nullptr;
		Render::JConstantBuffer* perFrameBuffer = nullptr;
		float aspectRatio = 1.0f;
	};

	JRenderServer() = default;

	JRenderDB& GetRenderDB() { return _renderDB; }
	const JRenderDB& GetRenderDB() const { return _renderDB; }

	void RegisterMaterial(JMaterial* material);
	void UnregisterMaterial(uint32 materialID);
	void MarkMaterialDirty(JMaterial* material);
	void RegisterCamera(JCameraComponent* camera, Render::JConstantBuffer* perFrameBuffer, float aspectRatio);
	void UnregisterCamera(uint32 cameraID);
	void MarkCameraDirty(JCameraComponent* camera);
	void Sync();

	bool BuildGraphicResource(uint32 materialID, Render::JShader* shader, Render::JGraphicResource& outResource) const;

private:
	uint32 FindMaterialIndex(uint32 materialID) const;
	JMaterial* FindMaterial(uint32 materialID) const;
	uint32 FindCameraIndex(uint32 cameraID) const;
	CameraRecord* FindCameraRecord(uint32 cameraID);
	const CameraRecord* FindCameraRecord(uint32 cameraID) const;

	JRenderDB _renderDB;
	std::vector<MaterialRecord> _materials;
	std::unordered_map<uint32, uint32> _materialIndexMap;
	std::vector<uint32> _dirtyMaterialIDs;
	std::vector<CameraRecord> _cameras;
	std::unordered_map<uint32, uint32> _cameraIndexMap;
	std::vector<uint32> _dirtyCameraIDs;
};

J_ENGINE_END

#endif
