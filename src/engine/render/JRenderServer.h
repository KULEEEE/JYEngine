#pragma once
#ifndef __J_RENDER_SERVER_H__
#define __J_RENDER_SERVER_H__

#include "engine/precompile.h"
#include "engine/render/JRenderDB.h"
#include "engine/render/JRenderSnapshot.h"
#include "engine/scene/JScene.h"
#include "engine/render/JRenderer.h"

/*#include "engine/asset/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }
/*#include "engine/render/JRenderTarget.h"*/ namespace J { namespace Engine { class JRenderTarget; } }

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
		JCameraHandle camera = {};
	};

	JRenderServer() = default;
	~JRenderServer();

	JRenderDB& GetRenderDB() { return _renderDB; }
	const JRenderDB& GetRenderDB() const { return _renderDB; }

	void RegisterMaterial(JMaterial* material);
	void UnregisterMaterial(uint32 materialID);
	void MarkMaterialDirty(JMaterial* material);
	void RegisterCamera(JCameraHandle camera);
	void UnregisterCamera(JCameraHandle camera);
	void MarkCameraDirty(JCameraHandle camera);
	void Sync();
	void SyncScene(JScene& scene);

	bool BuildFrameDesc(JRenderTarget* renderTarget, const JColor& clearColor, const Render::JViewport& viewport, const D3D12_RECT& scissorRect, JRenderer::FrameDesc& outFrameDesc) const;

private:
	uint32 FindMaterialIndex(uint32 materialID) const;
	JMaterial* FindMaterial(uint32 materialID) const;
	uint32 FindCameraIndex(JCameraHandle camera) const;
	CameraRecord* FindCameraRecord(JCameraHandle camera);
	const CameraRecord* FindCameraRecord(JCameraHandle camera) const;
	static uint64 MakeCameraKey(JCameraHandle camera);

	JRenderDB _renderDB;
	JFrameSnapshot _frameSnapshot;
	JCameraHandle _primaryCamera = {};
	std::vector<MaterialRecord> _materials;
	std::unordered_map<uint32, uint32> _materialIndexMap;
	std::vector<uint32> _dirtyMaterialIDs;
	std::vector<CameraRecord> _cameras;
	std::unordered_map<uint64, uint32> _cameraIndexMap;
	std::vector<uint64> _dirtyCameraKeys;
};

J_ENGINE_END

#endif
