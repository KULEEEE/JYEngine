#include "engine/render/JSceneColorPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JDrawItemCache.h"
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

	renderDrawItems(context, frameDesc.camera, frameDesc, frameDesc.opaqueDrawItemIndices);
	renderDrawItems(context, frameDesc.camera, frameDesc, frameDesc.transparentDrawItemIndices);

	context.commandQueue->EndRenderPass();
}

void JSceneColorPass::renderDrawItems(const JRenderPassContext& context, JCameraHandle camera, const JFrameDesc& frameDesc, const std::vector<uint32>& drawItemIndices)
{
	if (frameDesc.drawItemCache == nullptr)
	{
		return;
	}

	for (uint32 drawItemIndex : drawItemIndices)
	{
		if (drawItemIndex < frameDesc.drawItemCache->drawItems.size())
		{
			renderDrawItem(context, camera, frameDesc.drawItemCache->drawItems[drawItemIndex]);
		}
	}
}

void JSceneColorPass::renderDrawItem(const JRenderPassContext& context, JCameraHandle camera, const JDrawItem& drawItem)
{
	const JRenderDB::ResolvedDrawResources resources = context.renderDB->ResolveDrawResources(drawItem);
	if (!resources.IsValid())
	{
		++_lastStats.skippedDrawCount;
		return;
	}

	const JMeshResource* meshResource = resources.mesh;
	const JMaterialResource* materialResource = resources.material;
	const JTransformResource* transformResource = resources.transform;

	Render::JGraphicResource graphicResource(materialResource->shader);
	if (!context.renderDB->BuildGraphicResource(drawItem.material, materialResource->shader, graphicResource))
	{
		++_lastStats.skippedDrawCount;
#ifdef _DEBUG
		std::cerr << GetName() << " draw skipped: failed to build graphic resource." << std::endl;
#endif
		return;
	}

	const D3D12_GPU_VIRTUAL_ADDRESS objectGpuAddress = context.commandQueue->UploadFrameConstantBuffer(&transformResource->constants, sizeof(transformResource->constants));
	graphicResource.SetConstantBufferAddress("PerObject", objectGpuAddress);

	const JCameraResource* cameraResource = context.renderDB->FindCameraResource(camera);
	if (cameraResource != nullptr)
	{
		const D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = context.commandQueue->UploadFrameConstantBuffer(&cameraResource->constants, sizeof(cameraResource->constants));
		graphicResource.SetConstantBufferAddress("PerFrame", gpuAddress);
	}

	const JLightResource* lightResource = context.renderDB != nullptr ? context.renderDB->FindLightResource() : nullptr;
	if (lightResource != nullptr && lightResource->initialized)
	{
		const D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = context.commandQueue->UploadFrameConstantBuffer(&lightResource->constants, sizeof(lightResource->constants));
		if (graphicResource.SetConstantBufferAddress("PerLights", gpuAddress))
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
