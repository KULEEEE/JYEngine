#pragma once
#ifndef __J_RENDER_SNAPSHOT_H__
#define __J_RENDER_SNAPSHOT_H__

#include "engine/precompile.h"
#include "engine/render/JRenderFrame.h"
#include "engine/scene/JSceneHandle.h"

/*#include "engine/asset/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }
/*#include "engine/render/JRenderDefinition.h"*/ namespace J { namespace Render { struct JConstantBuffer; } }

J_ENGINE_BEGIN

struct JCameraSnapshot
{
	JCameraHandle camera = {};
	XMMATRIX viewProjection = XMMatrixIdentity();
};

struct JTransformSnapshot
{
	JTransformHandle transform = {};
	XMMATRIX world = XMMatrixIdentity();
};

struct JLightSnapshot
{
	struct Item
	{
		JVec4 colorIntensity = { 1.0f, 1.0f, 1.0f, 0.35f };
		JVec4 position = { 0.0f, 4.0f, -4.0f, 1.0f };
	};

	std::vector<Item> items;
};

struct JRenderObjectSnapshot
{
	JEntityHandle entity = {};
	JTransformHandle transform = {};
	JRenderObjectHandle renderObject = {};
	uint32 materialID = 0;
	const JMesh* mesh = nullptr;
	uint32 subMeshIndex = 0;
	bool transparent = false;
	bool visible = true;
	bool active = true;
};

struct JFrameSnapshot
{
	std::vector<JCameraSnapshot> cameras;
	std::vector<JTransformSnapshot> transforms;
	std::vector<JLightSnapshot> lights;
	std::vector<JRenderObjectSnapshot> renderObjects;
	std::vector<JDrawItem> opaqueDrawItems;
	std::vector<JDrawItem> transparentDrawItems;
};

J_ENGINE_END

#endif
