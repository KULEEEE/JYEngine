#include "engine/render/JGBufferPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JGraphicResource.h"
#include "engine/render/JMaterialResource.h"
#include "engine/render/JRenderDB.h"
#include "engine/asset/JShader.h"

#include <iostream>

J_ENGINE_BEGIN

namespace
{
	bool buildDrawGraphicResource(const JRenderPassContext& context, const JDrawItem& drawItem, Render::JGraphicResource& graphicResource)
	{
		if (!context.renderDB->BuildGraphicResource(drawItem.materialID, graphicResource.GetShader(), graphicResource))
		{
			return false;
		}

		const JRenderDB::TransformResource* transformResource = context.renderDB->FindTransformResource(drawItem.transform);
		if (transformResource != nullptr && transformResource->perObjectBuffer != nullptr)
		{
			graphicResource.SetConstantBuffer("PerObject", transformResource->perObjectBuffer);
		}

		const JRenderDB::LightResource* lightResource = context.renderDB->GetLightResource();
		if (lightResource != nullptr && lightResource->lightBuffer != nullptr)
		{
			graphicResource.SetConstantBuffer("PerLights", lightResource->lightBuffer);
		}

		return true;
	}
}

void JGBufferPass::Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc)
{
	_lastStats = {};
	_lastStats.name = GetName();

	if (context.commandQueue == nullptr || context.renderDB == nullptr || context.gBuffer == nullptr || !context.gBuffer->IsValid())
	{
		return;
	}

	JRenderTarget* albedoTarget = context.gBuffer->GetAlbedoTarget();
	context.commandQueue->BeginRenderPass(albedoTarget, JColor(0.0f, 0.0f, 0.0f, 1.0f), 0);
	context.commandQueue->SetViewports(1, &frameDesc.viewport);
	context.commandQueue->SetScissorRects(1, &frameDesc.scissorRect);

	for (const JDrawItem& drawItem : frameDesc.opaqueDrawItems)
	{
		if (drawItem.mesh == nullptr)
		{
			++_lastStats.skippedDrawCount;
			continue;
		}

		const JMeshResource* meshResource = context.renderDB->FindMeshResource(drawItem.mesh);
		const JMaterialResource* materialResource = context.renderDB->FindMaterialResource(drawItem.materialID);
		if (meshResource == nullptr || materialResource == nullptr || materialResource->GetShader() == nullptr || materialResource->GetPipeline() == nullptr)
		{
			++_lastStats.skippedDrawCount;
			continue;
		}

		Render::JGraphicResource graphicResource(materialResource->GetShader());
		if (!buildDrawGraphicResource(context, drawItem, graphicResource))
		{
			++_lastStats.skippedDrawCount;
			continue;
		}

		context.commandQueue->SetPipeline(materialResource->GetPipeline());
		context.commandQueue->SetGraphicResources(&graphicResource);
		context.commandQueue->BindVertexBuffer(meshResource);
		context.commandQueue->DrawIndexed(static_cast<uint32>(meshResource->indexSize), 1, 0, 0, 0);
		++_lastStats.drawCallCount;
	}

	context.commandQueue->EndRenderPass();
	context.commandQueue->TransitionRenderTarget(albedoTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

J_ENGINE_END