#pragma once
#ifndef __J_RENDER_PASS_H__
#define __J_RENDER_PASS_H__

#include "engine/precompile.h"
#include "engine/render/JRenderFrame.h"

/*#include "engine/render/JCommandQueue.h"*/ namespace J { namespace Render { class JCommandQueue; } }
/*#include "engine/render/JRenderContext.h"*/ namespace J { namespace Render { class JRenderContext; } }
/*#include "engine/render/JRenderDB.h"*/ namespace J { namespace Engine { class JRenderDB; } }
/*#include "engine/render/JGBuffer.h"*/ namespace J { namespace Engine { class JGBuffer; } }
/*#include "engine/render/JShadowMap.h"*/ namespace J { namespace Engine { class JShadowMap; } }

J_ENGINE_BEGIN

struct JRenderPassContext
{
	Render::JCommandQueue* commandQueue = nullptr;
	Render::JRenderContext* renderContext = nullptr;
	JRenderDB* renderDB = nullptr;
	JGBuffer* gBuffer = nullptr;
	JShadowMap* shadowMap = nullptr;
};

struct JRenderPassStats
{
	const char* name = "";
	uint32 drawCallCount = 0;
	uint32 skippedDrawCount = 0;
	uint32 lightBindingCount = 0;
};

class JRenderPass
{
public:
	virtual ~JRenderPass() = default;

	virtual const char* GetName() const = 0;
	virtual void Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc) = 0;
	virtual const JRenderPassStats& GetLastStats() const = 0;
};

J_ENGINE_END

#endif
