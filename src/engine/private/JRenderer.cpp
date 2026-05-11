#include "engine/JRenderer.h"

#include "engine/JCommandQueue.h"
#include "engine/JGraphicResource.h"
#include "engine/JRenderDB.h"
#include "engine/JRenderContext.h"
#include "engine/JMaterialResource.h"
#include "engine/asset/JShader.h"

#include <iostream>

J_ENGINE_BEGIN

namespace
{
	struct PerFrameConstants
	{
		XMFLOAT4X4 viewProjection;
	};
}

void JRenderer::Initialize(Render::JCommandQueue* commandQueue, Render::JRenderContext* renderContext, JRenderDB* renderDB)
{
	_commandQueue = commandQueue;
	_renderContext = renderContext;
	_renderDB = renderDB;
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

	const JRenderDB::CameraResource* cameraResource = _renderDB->FindCameraResource(frameDesc.cameraID);
	if (cameraResource == nullptr || cameraResource->perFrameBuffer == nullptr)
	{
		std::cerr << "JRenderer::Render skipped: camera resource is not ready." << std::endl;
		return;
	}

	PerFrameConstants perFrameConstants{};
	perFrameConstants.viewProjection = cameraResource->viewProjection;
	_renderContext->UpdateConstantBuffer(cameraResource->perFrameBuffer, &perFrameConstants, sizeof(perFrameConstants));

	_commandQueue->BeginRenderPass(frameDesc.renderTarget, frameDesc.clearColor, 0);
	_commandQueue->SetViewports(1, &frameDesc.viewport);
	_commandQueue->SetScissorRects(1, &frameDesc.scissorRect);

	for (const DrawItem& drawItem : frameDesc.drawItems)
	{
		if (drawItem.meshResource == nullptr)
		{
			std::cerr << "JRenderer::Render draw skipped: draw item is incomplete." << std::endl;
			continue;
		}

		const JMaterialResource* materialResource = _renderDB->FindMaterialResource(drawItem.materialID);
		if (materialResource == nullptr || materialResource->GetShader() == nullptr || materialResource->GetPipeline() == nullptr)
		{
			std::cerr << "JRenderer::Render draw skipped: material render data is incomplete." << std::endl;
			continue;
		}

		Render::JGraphicResource graphicResource(materialResource->GetShader());
		if (!_renderDB->BuildGraphicResource(drawItem.materialID, materialResource->GetShader(), graphicResource))
		{
			std::cerr << "JRenderer::Render draw skipped: failed to build graphic resource." << std::endl;
			continue;
		}

		_commandQueue->SetPipeline(materialResource->GetPipeline());
		_commandQueue->SetGraphicResources(&graphicResource);
		_commandQueue->BindVertexBuffer(drawItem.meshResource);
		_commandQueue->DrawIndexed(static_cast<uint32>(drawItem.meshResource->indexSize), 1, 0, 0, 0);
	}

	_commandQueue->EndRenderPass();
}

J_ENGINE_END
