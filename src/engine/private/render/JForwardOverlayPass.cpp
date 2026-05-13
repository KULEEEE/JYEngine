#include "engine/render/JForwardOverlayPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JGraphicResource.h"
#include "engine/render/JMaterialResource.h"
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

	context.commandQueue->BeginRenderPass(frameDesc.renderTarget, frameDesc.clearColor, 0, false);
	context.commandQueue->SetViewports(1, &frameDesc.viewport);
	context.commandQueue->SetScissorRects(1, &frameDesc.scissorRect);
	RenderDrawItems(context, frameDesc.transparentDrawItems);
	context.commandQueue->EndRenderPass();
}

void JForwardOverlayPass::RenderDrawItems(const JRenderPassContext& context, const std::vector<JDrawItem>& drawItems)
{
	for (const JDrawItem& drawItem : drawItems)
	{
		RenderDrawItem(context, drawItem);
	}
}

void JForwardOverlayPass::RenderDrawItem(const JRenderPassContext& context, const JDrawItem& drawItem)
{
	if (drawItem.mesh == nullptr)
	{
		++_lastStats.skippedDrawCount;
		return;
	}

	const JMeshResource* meshResource = context.renderDB->FindMeshResource(drawItem.mesh);
	const JMaterialResource* materialResource = context.renderDB->FindMaterialResource(drawItem.materialID);
	if (meshResource == nullptr || materialResource == nullptr || materialResource->GetShader() == nullptr || materialResource->GetPipeline() == nullptr)
	{
		++_lastStats.skippedDrawCount;
		return;
	}

	Render::JGraphicResource graphicResource(materialResource->GetShader());
	if (!context.renderDB->BuildGraphicResource(drawItem.materialID, materialResource->GetShader(), graphicResource))
	{
		++_lastStats.skippedDrawCount;
		return;
	}

	const JRenderDB::TransformResource* transformResource = context.renderDB->FindTransformResource(drawItem.transform);
	if (transformResource != nullptr && transformResource->perObjectBuffer != nullptr)
	{
		graphicResource.SetConstantBuffer("PerObject", transformResource->perObjectBuffer);
	}

	context.commandQueue->SetPipeline(materialResource->GetPipeline());
	context.commandQueue->SetGraphicResources(&graphicResource);
	context.commandQueue->BindVertexBuffer(meshResource);
	context.commandQueue->DrawIndexed(static_cast<uint32>(meshResource->indexSize), 1, 0, 0, 0);
	++_lastStats.drawCallCount;
}

J_ENGINE_END