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

	D3D12_GPU_VIRTUAL_ADDRESS cameraGpuAddress = 0;
	const JCameraResource* cameraResource = context.renderDB->FindCameraResource(frameDesc.camera);
	if (cameraResource != nullptr)
	{
		cameraGpuAddress = context.commandQueue->UploadFrameConstantBuffer(&cameraResource->constants, sizeof(cameraResource->constants));
	}

	D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress = 0;
	const JLightResource* lightResource = context.renderDB->FindLightResource();
	if (lightResource != nullptr && lightResource->initialized)
	{
		lightGpuAddress = context.commandQueue->UploadFrameConstantBuffer(&lightResource->constants, sizeof(lightResource->constants));
	}

	renderDrawItems(context, frameDesc, frameDesc.opaqueDrawItemIndices, cameraGpuAddress, lightGpuAddress);
	renderDrawItems(context, frameDesc, frameDesc.transparentDrawItemIndices, cameraGpuAddress, lightGpuAddress);

	context.commandQueue->EndRenderPass();
}

void JSceneColorPass::renderDrawItems(const JRenderPassContext& context, const JFrameDesc& frameDesc, JArrayView<const uint32> drawItemIndices, D3D12_GPU_VIRTUAL_ADDRESS cameraGpuAddress, D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress)
{
	if (frameDesc.drawItemCache == nullptr)
	{
		return;
	}

	for (uint32 drawItemIndex : drawItemIndices)
	{
		if (drawItemIndex < frameDesc.drawItemCache->drawItems.size())
		{
			renderDrawItem(context, frameDesc.drawItemCache->drawItems[drawItemIndex], cameraGpuAddress, lightGpuAddress);
		}
	}
}

void JSceneColorPass::renderDrawItem(const JRenderPassContext& context, const JDrawItem& drawItem, D3D12_GPU_VIRTUAL_ADDRESS cameraGpuAddress, D3D12_GPU_VIRTUAL_ADDRESS lightGpuAddress)
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

	if (cameraGpuAddress != 0)
	{
		graphicResource.SetConstantBufferAddress("PerFrame", cameraGpuAddress);
	}

	if (lightGpuAddress != 0)
	{
		if (graphicResource.SetConstantBufferAddress("PerLights", lightGpuAddress))
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
