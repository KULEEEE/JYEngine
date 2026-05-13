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
	JTransformHandle transform = {};
	JRenderObjectHandle renderObject = {};
	uint32 materialID = 0;
	const JMesh* mesh = nullptr;
	bool transparent = false;
};

struct JFrameDesc
{
	JCameraHandle camera = {};
	JRenderTarget* renderTarget = nullptr;
	JColor clearColor = JColors::DarkGray;
	Render::JViewport viewport = {};
	D3D12_RECT scissorRect = {};
	std::vector<JLightHandle> lights;
	std::vector<JDrawItem> opaqueDrawItems;
	std::vector<JDrawItem> transparentDrawItems;
};

J_ENGINE_END

#endif
