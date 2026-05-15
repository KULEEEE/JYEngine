#include "engine/render/JSceneColorPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JGraphicResource.h"
#include "engine/render/JMaterialResource.h"
#include "engine/render/JRenderDB.h"
#include "engine/scene/JLightSystem.h"
#include "engine/asset/JShader.h"

#include <iostream>

J_ENGINE_BEGIN

void JSceneColorPass::Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc)
{
	_lastStats = {};
	_lastStats.name = GetName();

	if (context.commandQueue == nullptr || context.renderDB == nullptr || frameDesc.renderTarget == nullptr)
	{
		return;
	}

	context.commandQueue->BeginRenderPass(frameDesc.renderTarget, frameDesc.clearColor, 0);
	context.commandQueue->SetViewports(1, &frameDesc.viewport);
	context.commandQueue->SetScissorRects(1, &frameDesc.scissorRect);

	RenderDrawItems(context, frameDesc.opaqueDrawItems);
	RenderDrawItems(context, frameDesc.transparentDrawItems);

	context.commandQueue->EndRenderPass();
}

void JSceneColorPass::RenderDrawItems(const JRenderPassContext& context, const std::vector<JDrawItem>& drawItems)
{
	for (const JDrawItem& drawItem : drawItems)
	{
		RenderDrawItem(context, drawItem);
	}
}

void JSceneColorPass::RenderDrawItem(const JRenderPassContext& context, const JDrawItem& drawItem)
{
	if (drawItem.mesh == nullptr)
	{
		++_lastStats.skippedDrawCount;
		std::cerr << GetName() << " draw skipped: draw item mesh is null." << std::endl;
		return;
	}

	const JMeshResource* meshResource = context.renderDB->FindMeshResource(drawItem.mesh);
	if (meshResource == nullptr)
	{
		++_lastStats.skippedDrawCount;
		std::cerr << GetName() << " draw skipped: mesh resource is not ready." << std::endl;
		return;
	}

	const JMaterialResource* materialResource = context.renderDB->FindMaterialResource(drawItem.materialID);
	if (materialResource == nullptr || materialResource->GetShader() == nullptr || materialResource->GetPipeline() == nullptr)
	{
		++_lastStats.skippedDrawCount;
		std::cerr << GetName() << " draw skipped: material render data is incomplete." << std::endl;
		return;
	}

	Render::JGraphicResource graphicResource(materialResource->GetShader());
	if (!context.renderDB->BuildGraphicResource(drawItem.materialID, materialResource->GetShader(), graphicResource))
	{
		++_lastStats.skippedDrawCount;
		std::cerr << GetName() << " draw skipped: failed to build graphic resource." << std::endl;
		return;
	}

	const JRenderDB::TransformResource* transformResource = context.renderDB->FindTransformResource(drawItem.transform);
	if (transformResource != nullptr && transformResource->perObjectBuffer != nullptr)
	{
		graphicResource.SetConstantBuffer("PerObject", transformResource->perObjectBuffer);
	}

	const JLightResource* lightResource = context.lightSystem != nullptr ? context.lightSystem->GetLightResource() : nullptr;
	if (lightResource != nullptr && lightResource->lightBuffer != nullptr)
	{
		if (graphicResource.SetConstantBuffer("PerLights", lightResource->lightBuffer))
		{
			++_lastStats.lightBindingCount;
		}
	}

	context.commandQueue->SetPipeline(materialResource->GetPipeline());
	context.commandQueue->SetGraphicResources(&graphicResource);
	context.commandQueue->BindVertexBuffer(meshResource);
	context.commandQueue->DrawIndexed(static_cast<uint32>(meshResource->indexSize), 1, 0, 0, 0);
	++_lastStats.drawCallCount;
}

J_ENGINE_END
