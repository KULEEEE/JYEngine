#pragma once
#ifndef __J_RENDER_FRAME_H__
#define __J_RENDER_FRAME_H__

#include "engine/precompile.h"
#include "engine/render/JRenderDefinition.h"
#include "engine/scene/JScene.h"

/*#include "engine/render/JRenderTarget.h"*/ namespace J { namespace Engine { class JRenderTarget; } }
/*#include "engine/asset/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }
namespace J { namespace Engine { struct JDrawItemCache; } }

J_ENGINE_BEGIN

struct JDrawItem
{
	JEntityHandle entity = {};
	JRenderObjectComponentHandle renderObject = {};
	JTransformHandle transform = {};
	uint32 materialID = 0;
	const JMesh* mesh = nullptr;
	uint32 meshResourceIndex = static_cast<uint32>(-1);
	uint32 materialResourceIndex = static_cast<uint32>(-1);
	uint32 transformResourceIndex = static_cast<uint32>(-1);
	uint32 subMeshIndex = 0;
	uint32 indexCount = 0;
	uint32 startIndex = 0;
	bool transparent = false;
};

struct JFrameTransformSnapshot
{
	JTransformHandle transform = {};
	XMMATRIX world = XMMatrixIdentity();
};

struct JFrameLightSnapshot
{
	struct Item
	{
		JVec4 colorIntensity = { 1.0f, 1.0f, 1.0f, 0.35f };
		JVec4 position = { 0.0f, 4.0f, -4.0f, 1.0f };
	};

	std::vector<Item> items;
};

struct JFrameDesc
{
	JCameraHandle camera = {};
	JRenderTarget* renderTarget = nullptr;
	JColor clearColor = JColors::DarkGray;
	Render::JViewport viewport = {};
	D3D12_RECT scissorRect = {};
	const JDrawItemCache* drawItemCache = nullptr;
	XMMATRIX cameraViewProjection = XMMatrixIdentity();
	std::vector<JFrameTransformSnapshot> transformSnapshots;
	JFrameLightSnapshot lightSnapshot;
	std::vector<uint32> opaqueDrawItemIndices;
	std::vector<uint32> transparentDrawItemIndices;
	uint32 cullingTestedDrawItemCount = 0;
	uint32 culledDrawItemCount = 0;
};

J_ENGINE_END

#endif
