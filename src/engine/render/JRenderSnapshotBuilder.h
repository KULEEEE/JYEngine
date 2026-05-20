#pragma once
#ifndef __J_RENDER_SNAPSHOT_BUILDER_H__
#define __J_RENDER_SNAPSHOT_BUILDER_H__

#include "engine/precompile.h"
#include "engine/render/JRenderFrame.h"
#include "engine/render/JRenderSnapshot.h"
#include "engine/scene/JScene.h"

J_ENGINE_BEGIN

class JRenderSnapshotBuilder
{
public:
	struct Input
	{
		JScene* scene = nullptr;
		const std::vector<JCameraHandle>* cameras = nullptr;
	};

	struct Result
	{
		std::unordered_set<uint64> activeCameraKeys;
		std::unordered_set<uint64> activeTransformKeys;
		std::unordered_set<const JMesh*> activeMeshes;
	};

	static void Build(const Input& input, JFrameSnapshot& outSnapshot, Result& outResult);

private:
	static void resetOutput(JFrameSnapshot& outSnapshot, Result& outResult);
	static void collectActiveSceneResourceKeys(const JScene& scene, const std::vector<JCameraHandle>& cameras, Result& outResult);
	static void buildCameraSnapshots(const JScene& scene, const std::vector<JCameraHandle>& cameras, JFrameSnapshot& outSnapshot);
	static void buildTransformSnapshots(JScene& scene, JFrameSnapshot& outSnapshot);
	static void buildLightSnapshots(const JScene& scene, JFrameSnapshot& outSnapshot);
};

J_ENGINE_END

#endif
