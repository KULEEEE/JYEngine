#include "engine/render/JRenderer.h"

#include "engine/render/JRenderDB.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JRenderPass.h"
#include "engine/render/JRenderTarget.h"
#include "engine/render/JSceneColorPass.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JGBufferPass.h"
#include "engine/render/JLightingPass.h"
#include "engine/render/JForwardOverlayPass.h"

#include <iostream>

J_ENGINE_BEGIN

JRenderer::~JRenderer() = default;

void JRenderer::initializeDefaultPasses()
{
	if (_renderPath == RenderPath::Deferred)
	{
		initializeDeferredPasses();
		return;
	}

	initializeForwardPasses();
}

void JRenderer::initializeForwardPasses()
{
	_passes.clear();
	_passes.push_back(std::make_unique<JSceneColorPass>());
}

void JRenderer::initializeDeferredPasses()
{
	_passes.clear();
	_passes.push_back(std::make_unique<JGBufferPass>());
	_passes.push_back(std::make_unique<JLightingPass>());
	_passes.push_back(std::make_unique<JForwardOverlayPass>());
}

void JRenderer::Initialize(Render::JCommandQueue* commandQueue, Render::JRenderContext* renderContext, JRenderDB* renderDB)
{
	_commandQueue = commandQueue;
	_renderContext = renderContext;
	_renderDB = renderDB;
	initializeDefaultPasses();
}

void JRenderer::SetRenderPath(RenderPath renderPath)
{
	if (_renderPath == renderPath)
	{
		return;
	}

	_renderPath = renderPath;
	initializeDefaultPasses();
}

void JRenderer::ensureGBuffer(const FrameDesc& frameDesc)
{
	if (_renderPath != RenderPath::Deferred || frameDesc.renderTarget == nullptr)
	{
		return;
	}

	const uint32 width = std::max(frameDesc.renderTarget->GetWidth(), 1u);
	const uint32 height = std::max(frameDesc.renderTarget->GetHeight(), 1u);
	if (_gBuffer != nullptr && _gBuffer->IsValid() && _gBuffer->GetWidth() == width && _gBuffer->GetHeight() == height)
	{
		return;
	}

	JGBuffer::Desc desc;
	desc.width = width;
	desc.height = height;
	_gBuffer = std::make_unique<JGBuffer>();
	_gBuffer->Initialize(desc);
}

void JRenderer::Render(const FrameDesc& frameDesc)
{
	if (_commandQueue == nullptr || _renderContext == nullptr || _renderDB == nullptr)
	{
		std::cerr << "JRenderer::Render skipped: command queue, render context, or render DB is null." << std::endl;
		return;
	}

	if (frameDesc.renderTarget == nullptr)
	{
		std::cerr << "JRenderer::Render skipped: render target is null." << std::endl;
		return;
	}

	if (_passes.empty())
	{
		initializeDefaultPasses();
	}
	ensureGBuffer(frameDesc);

	const JCameraResource* cameraResource = _renderDB->FindCameraResource(frameDesc.camera);
	if (cameraResource == nullptr || cameraResource->perFrameBuffer == nullptr)
	{
		std::cerr << "JRenderer::Render skipped: camera resource is not ready." << std::endl;
		return;
	}

	JRenderPassContext context;
	context.commandQueue = _commandQueue;
	context.renderContext = _renderContext;
	context.renderDB = _renderDB;
	context.gBuffer = _gBuffer.get();

	for (const std::unique_ptr<JRenderPass>& pass : _passes)
	{
		if (pass != nullptr)
		{
			pass->Execute(context, frameDesc);
		}
	}
}

J_ENGINE_END
