#pragma once
#ifndef __J_RENDERER_H__
#define __J_RENDERER_H__

#include "engine/precompile.h"
#include "engine/JRenderDefinition.h"
#include "engine/JRenderResource.h"

/*#include "engine/JCommandQueue.h"*/ namespace J { namespace Render { class JCommandQueue; } }
/*#include "engine/JRenderContext.h"*/ namespace J { namespace Render { class JRenderContext; } }
/*#include "engine/JRenderDB.h"*/ namespace J { namespace Engine { class JRenderDB; } }
/*#include "engine/JRenderTarget.h"*/ namespace J { namespace Engine { class JRenderTarget; } }
/*#include "engine/asset/JShader.h"*/ namespace J { namespace Render { class JShader; } }

J_ENGINE_BEGIN

class JRenderer
{
public:
	struct DrawItem
	{
		uint32 materialID = 0;
		const JMeshResource* meshResource = nullptr;
	};

	struct FrameDesc
	{
		uint32 cameraID = 0;
		JRenderTarget* renderTarget = nullptr;
		JColor clearColor = JColors::DarkGray;
		Render::JViewport viewport = {};
		D3D12_RECT scissorRect = {};
		std::vector<DrawItem> drawItems;
	};

	JRenderer() = default;

	void Initialize(Render::JCommandQueue* commandQueue, Render::JRenderContext* renderContext, JRenderDB* renderDB);
	void SetRenderDB(JRenderDB* renderDB) { _renderDB = renderDB; }
	JRenderDB* GetRenderDB() const { return _renderDB; }

	void Render(const FrameDesc& frameDesc);

private:
	Render::JCommandQueue* _commandQueue = nullptr;
	Render::JRenderContext* _renderContext = nullptr;
	JRenderDB* _renderDB = nullptr;
};

J_ENGINE_END

#endif
