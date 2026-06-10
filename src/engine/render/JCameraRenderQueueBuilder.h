#pragma once
#ifndef __J_CAMERA_RENDER_QUEUE_BUILDER_H__
#define __J_CAMERA_RENDER_QUEUE_BUILDER_H__

#include "engine/precompile.h"
#include "engine/asset/JMesh.h"
#include "engine/render/JDrawItemCache.h"
#include "engine/render/JRenderSnapshot.h"

J_ENGINE_BEGIN

class JJobSystem;
class JRenderDB;
class JScene;

class JCameraRenderQueueBuilder
{
public:
	struct Input
	{
		const JDrawItemCache* drawItemCache = nullptr;
		const JRenderDB* renderDB = nullptr;
		const JScene* scene = nullptr;
		JJobSystem* jobSystem = nullptr;
	};

	static void Build(const Input& input, JFrameSnapshot& snapshot);

private:
	static JFrustum buildFrustum(const XMMATRIX& viewProjection);
	static bool isVisible(const JCameraSnapshot& cameraSnapshot, const JDrawItem& drawItem, const Input& input);
	static bool isAABBVisible(const JFrustum& frustum, const JMesh::Bounds& bounds, const XMMATRIX& world);
	static void buildSerial(const Input& input, JCameraSnapshot& cameraSnapshot);
	static void buildParallel(const Input& input, JCameraSnapshot& cameraSnapshot);
};

J_ENGINE_END

#endif
