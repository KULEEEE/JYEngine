#include "engine/render/JSceneColorPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JGraphicResource.h"
#include "engine/render/JRenderDB.h"
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

	renderDrawItems(context, frameDesc.camera, frameDesc.opaqueDrawItems);
	renderDrawItems(context, frameDesc.camera, frameDesc.transparentDrawItems);

	context.commandQueue->EndRenderPass();
}

void JSceneColorPass::renderDrawItems(const JRenderPassContext& context, JCameraHandle camera, const std::vector<JDrawItem>& drawItems)
{
	for (const JDrawItem& drawItem : drawItems)
	{
		renderDrawItem(context, camera, drawItem);
	}
}

void JSceneColorPass::renderDrawItem(const JRenderPassContext& context, JCameraHandle camera, const JDrawItem& drawItem)
{
	const JMeshResource* meshResource = context.renderDB->FindMeshResource(drawItem.mesh);
	if (meshResource == nullptr)
	{
		++_lastStats.skippedDrawCount;
		std::cerr << GetName() << " draw skipped: draw item mesh is null." << std::endl;
		return;
	}

	const JMaterialResource* materialResource = context.renderDB->FindMaterialResource(drawItem.materialID);
	if (materialResource == nullptr || materialResource->shader == nullptr || materialResource->pipeline == nullptr)
	{
		++_lastStats.skippedDrawCount;
		std::cerr << GetName() << " draw skipped: material render data is incomplete." << std::endl;
		return;
	}

	Render::JGraphicResource graphicResource(materialResource->shader);
	if (!context.renderDB->BuildGraphicResource(drawItem.materialID, materialResource->shader, graphicResource))
	{
		++_lastStats.skippedDrawCount;
		std::cerr << GetName() << " draw skipped: failed to build graphic resource." << std::endl;
		return;
	}

	const JTransformResource* transformResource = context.renderDB->FindTransformResource(drawItem.transform);
	if (transformResource != nullptr && transformResource->perObjectBuffer != nullptr)
	{
		graphicResource.SetConstantBuffer("PerObject", transformResource->perObjectBuffer);
	}

	const JCameraResource* cameraResource = context.renderDB->FindCameraResource(camera);
	if (cameraResource != nullptr && cameraResource->perFrameBuffer != nullptr)
	{
		graphicResource.SetConstantBuffer("PerFrame", cameraResource->perFrameBuffer);
	}

	const JLightResource* lightResource = context.renderDB != nullptr ? context.renderDB->FindLightResource() : nullptr;
	if (lightResource != nullptr && lightResource->lightBuffer != nullptr)
	{
		if (graphicResource.SetConstantBuffer("PerLights", lightResource->lightBuffer))
		{
			++_lastStats.lightBindingCount;
		}
	}

	context.commandQueue->SetPipeline(materialResource->pipeline);
	context.commandQueue->SetGraphicResources(&graphicResource);
	context.commandQueue->BindVertexBuffer(meshResource);
	const uint32 indexCount = drawItem.indexCount != 0 ? drawItem.indexCount : static_cast<uint32>(meshResource->indexSize);
	context.commandQueue->DrawIndexed(indexCount, 1, drawItem.startIndex, 0, 0);
	++_lastStats.drawCallCount;
}

J_ENGINE_END
