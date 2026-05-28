#include "engine/render/JDepthPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JDrawItemCache.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JGraphicResource.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JRenderDB.h"
#include "engine/asset/JShader.h"

J_ENGINE_BEGIN

namespace
{
	bool buildDepthGraphicResource(const JRenderPassContext& context, JCameraHandle camera, const JDrawItem& drawItem, Render::JGraphicResource& graphicResource)
	{
		if (!context.renderDB->BuildGraphicResource(drawItem.material, graphicResource.GetShader(), graphicResource))
		{
			return false;
		}

		const JTransformResource* transformResource = context.renderDB->FindTransformResource(drawItem.transform);
		if (transformResource != nullptr)
		{
			const D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = context.commandQueue->UploadFrameConstantBuffer(&transformResource->constants, sizeof(transformResource->constants));
			graphicResource.SetConstantBufferAddress("PerObject", gpuAddress);
		}

		const JCameraResource* cameraResource = context.renderDB->FindCameraResource(camera);
		if (cameraResource != nullptr)
		{
			const D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = context.commandQueue->UploadFrameConstantBuffer(&cameraResource->constants, sizeof(cameraResource->constants));
			graphicResource.SetConstantBufferAddress("PerFrame", gpuAddress);
		}

		return true;
	}
}

JDepthPass::~JDepthPass()
{
	delete _pipeline;
	delete _shader;
}

bool JDepthPass::ensureResources(const JRenderPassContext& context)
{
	if (_shader != nullptr && _pipeline != nullptr)
	{
		return true;
	}

	if (context.renderContext == nullptr)
	{
		return false;
	}

	if (_shader == nullptr)
	{
		const std::string shaderPath = get_Engine_Shader_Path() + "\\depth_prepass.hlsl";
		_shader = context.renderContext->CreateShader(shaderPath);
	}

	if (_shader != nullptr && _pipeline == nullptr)
	{
		_pipeline = context.renderContext->CreatePipeline(
			_shader,
			false,
			true,
			false,
			false,
			{},
			true,
			D3D12_DEPTH_WRITE_MASK_ALL,
			D3D12_COMPARISON_FUNC_GREATER,
			false);
	}

	return _shader != nullptr && _pipeline != nullptr;
}

void JDepthPass::Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc)
{
	_lastStats = {};
	_lastStats.name = GetName();

	if (context.commandQueue == nullptr || context.renderDB == nullptr || context.gBuffer == nullptr || !context.gBuffer->IsValid())
	{
		return;
	}

	if (!ensureResources(context))
	{
		return;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = context.gBuffer->GetDSVHandle();
	context.commandQueue->BeginDepthPass(&dsvHandle, 0, true);
	context.commandQueue->SetViewports(1, &frameDesc.viewport);
	context.commandQueue->SetScissorRects(1, &frameDesc.scissorRect);

	if (frameDesc.drawItemCache == nullptr)
	{
		context.commandQueue->EndRenderPass();
		return;
	}

	for (uint32 drawItemIndex : frameDesc.opaqueDrawItemIndices)
	{
		if (drawItemIndex >= frameDesc.drawItemCache->drawItems.size())
		{
			continue;
		}

		const JDrawItem& drawItem = frameDesc.drawItemCache->drawItems[drawItemIndex];
		const JRenderDB::ResolvedDrawResources resources = context.renderDB->ResolveDrawResources(drawItem);
		if (!resources.IsValid())
		{
			++_lastStats.skippedDrawCount;
			continue;
		}

		Render::JGraphicResource graphicResource(_shader);
		if (!buildDepthGraphicResource(context, frameDesc.camera, drawItem, graphicResource))
		{
			++_lastStats.skippedDrawCount;
			continue;
		}

		context.commandQueue->SetPipeline(_pipeline);
		context.commandQueue->SetGraphicResources(&graphicResource);
		context.commandQueue->BindVertexBuffer(resources.mesh);
		const uint32 indexCount = drawItem.indexCount != 0 ? drawItem.indexCount : static_cast<uint32>(resources.mesh->indexSize);
		context.commandQueue->DrawIndexed(indexCount, 1, drawItem.startIndex, 0, 0);
		++_lastStats.drawCallCount;
	}

	context.commandQueue->EndRenderPass();
}

J_ENGINE_END
