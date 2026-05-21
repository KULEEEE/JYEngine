#pragma once
#ifndef __J_RENDER_FRAME_H__
#define __J_RENDER_FRAME_H__

#include "engine/precompile.h"
#include "engine/render/JRenderDefinition.h"
#include "engine/scene/JScene.h"

/*#include "engine/render/JRenderTarget.h"*/ namespace J { namespace Engine { class JRenderTarget; } }
/*#include "engine/asset/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }

J_ENGINE_BEGIN

struct JDrawItem
{
	JEntityHandle entity = {};
	JRenderObjectComponentHandle renderObject = {};
	JTransformHandle transform = {};
	uint32 materialID = 0;
	const JMesh* mesh = nullptr;
	uint32 subMeshIndex = 0;
	uint32 indexCount = 0;
	uint32 startIndex = 0;
	bool transparent = false;
};

struct JFrameDesc
{
	JCameraHandle camera = {};
	JRenderTarget* renderTarget = nullptr;
	JColor clearColor = JColors::DarkGray;
	Render::JViewport viewport = {};
	D3D12_RECT scissorRect = {};
	std::vector<JDrawItem> opaqueDrawItems;
	std::vector<JDrawItem> transparentDrawItems;
	uint32 cullingTestedDrawItemCount = 0;
	uint32 culledDrawItemCount = 0;
	std::vector<std::string> debugOverlayLines;
};

J_ENGINE_END

#endif
