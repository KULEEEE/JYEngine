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

	void renderDrawItems(Render::JCommandQueue* commandQueue, JRenderDB* renderDB, const std::vector<JRenderer::DrawItem>& drawItems)
	{
		for (const JRenderer::DrawItem& drawItem : drawItems)
		{
			if (drawItem.mesh == nullptr)
			{
				std::cerr << "JRenderer::Render draw skipped: draw item mesh is null." << std::endl;
				continue;
			}

			const JMeshResource* meshResource = renderDB->FindMeshResource(drawItem.mesh);
			if (meshResource == nullptr)
			{
				std::cerr << "JRenderer::Render draw skipped: mesh resource is not ready." << std::endl;
				continue;
			}

			const JMaterialResource* materialResource = renderDB->FindMaterialResource(drawItem.materialID);
			if (materialResource == nullptr || materialResource->GetShader() == nullptr || materialResource->GetPipeline() == nullptr)
			{
				std::cerr << "JRenderer::Render draw skipped: material render data is incomplete." << std::endl;
				continue;
			}

			Render::JGraphicResource graphicResource(materialResource->GetShader());
			if (!renderDB->BuildGraphicResource(drawItem.materialID, materialResource->GetShader(), graphicResource))
			{
				std::cerr << "JRenderer::Render draw skipped: failed to build graphic resource." << std::endl;
				continue;
			}

			const JRenderDB::TransformResource* transformResource = renderDB->FindTransformResource(drawItem.transform);
			if (transformResource != nullptr && transformResource->perObjectBuffer != nullptr)
			{
				graphicResource.SetConstantBuffer("PerObject", transformResource->perObjectBuffer);
			}

			const JRenderDB::LightResource* lightResource = renderDB->GetLightResource();
			if (lightResource != nullptr && lightResource->lightBuffer != nullptr)
			{
				graphicResource.SetConstantBuffer("PerLights", lightResource->lightBuffer);
			}

			commandQueue->SetPipeline(materialResource->GetPipeline());
			commandQueue->SetGraphicResources(&graphicResource);
			commandQueue->BindVertexBuffer(meshResource);
			commandQueue->DrawIndexed(static_cast<uint32>(meshResource->indexSize), 1, 0, 0, 0);
		}
	}
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

	const JRenderDB::CameraResource* cameraResource = _renderDB->FindCameraResource(frameDesc.camera);
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

	renderDrawItems(_commandQueue, _renderDB, frameDesc.opaqueDrawItems);
	renderDrawItems(_commandQueue, _renderDB, frameDesc.transparentDrawItems);

	_commandQueue->EndRenderPass();
}

J_ENGINE_END
