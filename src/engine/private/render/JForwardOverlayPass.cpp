#include "engine/render/JForwardOverlayPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JGraphicResource.h"
#include "engine/render/JRenderDB.h"
#include "engine/asset/JShader.h"

#include <iostream>

J_ENGINE_BEGIN

void JForwardOverlayPass::Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc)
{
	_lastStats = {};
	_lastStats.name = GetName();

	if (context.commandQueue == nullptr || context.renderDB == nullptr || frameDesc.renderTarget == nullptr || frameDesc.transparentDrawItems.empty())
	{
		return;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = context.gBuffer != nullptr ? context.gBuffer->GetDSVHandle() : D3D12_CPU_DESCRIPTOR_HANDLE{};
	context.commandQueue->BeginRenderPass(frameDesc.renderTarget, frameDesc.clearColor, 0, context.gBuffer != nullptr ? &dsvHandle : nullptr, false, false);
	context.commandQueue->SetViewports(1, &frameDesc.viewport);
	context.commandQueue->SetScissorRects(1, &frameDesc.scissorRect);
	renderDrawItems(context, frameDesc.camera, frameDesc.transparentDrawItems);
	context.commandQueue->EndRenderPass();
}

void JForwardOverlayPass::renderDrawItems(const JRenderPassContext& context, JCameraHandle camera, const std::vector<JDrawItem>& drawItems)
{
	for (const JDrawItem& drawItem : drawItems)
	{
		renderDrawItem(context, camera, drawItem);
	}
}

void JForwardOverlayPass::renderDrawItem(const JRenderPassContext& context, JCameraHandle camera, const JDrawItem& drawItem)
{
	if (drawItem.meshResource == nullptr)
	{
		++_lastStats.skippedDrawCount;
		return;
	}

	const JMeshResource* meshResource = drawItem.meshResource;
	const JMaterialResource* materialResource = drawItem.materialResource;
	if (meshResource == nullptr || materialResource == nullptr || materialResource->shader == nullptr || materialResource->pipeline == nullptr)
	{
		++_lastStats.skippedDrawCount;
		return;
	}

	Render::JGraphicResource graphicResource(materialResource->shader);
	if (!context.renderDB->BuildGraphicResource(drawItem.materialID, materialResource->shader, graphicResource))
	{
		++_lastStats.skippedDrawCount;
		return;
	}

	const JTransformResource* transformResource = drawItem.transformResource;
	if (transformResource != nullptr && transformResource->perObjectBuffer != nullptr)
	{
		graphicResource.SetConstantBuffer("PerObject", transformResource->perObjectBuffer);
	}

	const JCameraResource* cameraResource = context.renderDB->FindCameraResource(camera);
	if (cameraResource != nullptr && cameraResource->perFrameBuffer != nullptr)
	{
		graphicResource.SetConstantBuffer("PerFrame", cameraResource->perFrameBuffer);
	}

	context.commandQueue->SetPipeline(materialResource->pipeline);
	context.commandQueue->SetGraphicResources(&graphicResource);
	context.commandQueue->BindVertexBuffer(meshResource);
	context.commandQueue->DrawIndexed(static_cast<uint32>(meshResource->indexSize), 1, 0, 0, 0);
	++_lastStats.drawCallCount;
}

J_ENGINE_END
