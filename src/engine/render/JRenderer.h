#pragma once
#ifndef __J_RENDERER_H__
#define __J_RENDERER_H__

#include "engine/precompile.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JRenderDB.h"
#include "engine/render/JRenderFrame.h"
#include "engine/render/JRenderPass.h"
#include "engine/render/JShadowMap.h"

/*#include "engine/render/JCommandQueue.h"*/ namespace J { namespace Render { class JCommandQueue; } }
/*#include "engine/render/JRenderContext.h"*/ namespace J { namespace Render { class JRenderContext; } }
/*#include "engine/asset/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }

J_ENGINE_BEGIN

class JRenderer
{
public:
	enum class RenderPath
	{
		Forward,
		Deferred
	};

	using DrawItem = JDrawItem;
	using FrameDesc = JFrameDesc;

	struct PassTiming
	{
		const char* name = "";
		double cpuMs = 0.0;
		uint32 drawCalls = 0;
		uint32 skipped = 0;
	};

	JRenderer() = default;
	~JRenderer();

	void Initialize(Render::JCommandQueue* commandQueue, Render::JRenderContext* renderContext);
	JRenderDB& GetRenderDB() { return _renderDB; }
	const JRenderDB& GetRenderDB() const { return _renderDB; }
	void SetRenderPath(RenderPath renderPath);
	RenderPath GetRenderPath() const { return _renderPath; }
	JGBuffer* GetGBuffer() const { return _gBuffer.get(); }

	const std::vector<PassTiming>& GetLastFrameTimings() const { return _lastFrameTimings; }

	void Render(const FrameDesc& frameDesc);

private:
	void initializeDefaultPasses();
	void initializeForwardPasses();
	void initializeDeferredPasses();
	void ensureGBuffer(const FrameDesc& frameDesc);
	void ensureShadowMap();
	void prepareFrameResources(const FrameDesc& frameDesc);

	Render::JCommandQueue* _commandQueue = nullptr;
	Render::JRenderContext* _renderContext = nullptr;
	JRenderDB _renderDB;
	std::vector<std::unique_ptr<JRenderPass>> _passes;
	std::unique_ptr<JGBuffer> _gBuffer;
	std::unique_ptr<JShadowMap> _shadowMap;
	RenderPath _renderPath = RenderPath::Forward;
	std::vector<PassTiming> _lastFrameTimings;
};

J_ENGINE_END

#endif
