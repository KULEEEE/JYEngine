#pragma once
#ifndef __J_RENDER_SNAPSHOT_H__
#define __J_RENDER_SNAPSHOT_H__

#include "engine/precompile.h"
#include "engine/render/JRenderFrame.h"
#include "engine/scene/JSceneHandle.h"

/*#include "engine/asset/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }
/*#include "engine/render/JRenderDefinition.h"*/ namespace J { namespace Render { struct JConstantBuffer; } }

J_ENGINE_BEGIN

struct JFrustum
{
	XMVECTOR planes[6] = {};
};

struct JCameraSnapshot
{
	JCameraHandle camera = {};
	XMMATRIX viewProjection = XMMatrixIdentity();
	XMMATRIX inverseViewProjection = XMMatrixIdentity();
	JVec4 worldPosition = { 0.0f, 0.0f, 0.0f, 1.0f };
	JFrustum frustum = {};
	uint32 cullingTestedDrawItemCount = 0;
	uint32 culledDrawItemCount = 0;
	std::vector<uint32> opaqueDrawItemIndices;
	std::vector<uint32> transparentDrawItemIndices;
};

using JTransformSnapshot = JFrameTransformSnapshot;
using JLightSnapshot = JFrameLightSnapshot;

struct JFrameSnapshot
{
	std::vector<JCameraSnapshot> cameras;
	std::vector<JTransformSnapshot> transforms;
	std::vector<JLightSnapshot> lights;
};

J_ENGINE_END

#endif
