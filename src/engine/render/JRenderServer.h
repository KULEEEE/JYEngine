#pragma once
#ifndef __J_RENDER_SERVER_H__
#define __J_RENDER_SERVER_H__

#include "engine/precompile.h"
#include "engine/render/JDrawItemCache.h"
#include "engine/render/JRenderSnapshot.h"
#include "engine/render/JRenderSnapshotBuilder.h"
#include "engine/scene/JScene.h"
#include "engine/render/JRenderer.h"

/*#include "engine/core/JJobSystem.h"*/ namespace J { namespace Engine { class JJobSystem; } }
/*#include "engine/render/JRenderTarget.h"*/ namespace J { namespace Engine { class JRenderTarget; } }

J_ENGINE_BEGIN

class JRenderServer
{
public:
	JRenderServer() = default;
	~JRenderServer();

	void SetJobSystem(JJobSystem* jobSystem) { _jobSystem = jobSystem; }

	void SyncScene(JScene& scene);

	bool BuildFrameDesc(JRenderTarget* renderTarget, JRenderer::FrameDesc& outFrameDesc);

private:
	const JCameraSnapshot* findCameraSnapshot(JCameraHandle camera) const;
	std::vector<JCameraHandle> collectSceneCameraHandles(const JScene& scene) const;
	void updateDrawItemCache(JScene& scene, const std::vector<JSceneRenderObjectEvent>& events, JRenderSnapshotBuilder::Result& outResult);
	void rebuildDrawItemCache(const JScene& scene, JRenderSnapshotBuilder::Result& outResult);
	void appendDrawItems(const JScene& scene, JRenderObjectComponentHandle renderObject, JRenderSnapshotBuilder::Result& outResult);
	void patchDrawItems(const JScene& scene, JRenderObjectComponentHandle renderObject, JRenderSnapshotBuilder::Result& outResult);

	JJobSystem* _jobSystem = nullptr;
	JFrameSnapshot _frameSnapshot;
	std::vector<JFrameMaterialSnapshot> _frameMaterialSnapshots;
	JDrawItemCache _drawItemCache;
	JScene* _syncedScene = nullptr;
	JCameraHandle _primaryCamera = {};
};

J_ENGINE_END

#endif
