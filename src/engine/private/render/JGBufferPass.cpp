#include "engine/render/JGBufferPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JDrawItemCache.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JGraphicResource.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JRenderDB.h"
#include "engine/asset/JShader.h"

#include <iostream>

J_ENGINE_BEGIN

namespace
{
	bool buildDrawGraphicResource(const JRenderPassContext& context, const JDrawItem& drawItem, Render::JGraphicResource& graphicResource, D3D12_GPU_VIRTUAL_ADDRESS cameraGpuAddress)
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

		if (cameraGpuAddress != 0)
		{
			graphicResource.SetConstantBufferAddress("PerFrame", cameraGpuAddress);
		}

		return true;
	}
}

JGBufferPass::~JGBufferPass()
{
	delete _pipeline;
	delete _shader;
}

bool JGBufferPass::ensureResources(const JRenderPassContext& context)
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
		const std::string shaderPath = get_Engine_Shader_Path() + "\\gbuffer_albedo.hlsl";
		_shader = context.renderContext->CreateShader(shaderPath);
	}

	if (_shader != nullptr && _pipeline == nullptr)
	{
		const std::vector<DXGI_FORMAT> gBufferFormats =
		{
			context.gBuffer != nullptr ? context.gBuffer->GetDesc().albedoFormat : DXGI_FORMAT_R8G8B8A8_UNORM,
			context.gBuffer != nullptr ? context.gBuffer->GetDesc().normalFormat : DXGI_FORMAT_R16G16B16A16_FLOAT,
			context.gBuffer != nullptr ? context.gBuffer->GetDesc().materialFormat : DXGI_FORMAT_R8G8B8A8_UNORM
		};
		_pipeline = context.renderContext->CreatePipeline(
			_shader,
			false,
			true,
			true,
			true,
			gBufferFormats,
			true,
			D3D12_DEPTH_WRITE_MASK_ZERO,
			D3D12_COMPARISON_FUNC_EQUAL);
	}

	return _shader != nullptr && _pipeline != nullptr;
}

void JGBufferPass::Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc)
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

	JRenderTarget* albedoTarget = context.gBuffer->GetAlbedoTarget();
	JRenderTarget* normalTarget = context.gBuffer->GetNormalTarget();
	JRenderTarget* materialTarget = context.gBuffer->GetMaterialTarget();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = context.gBuffer->GetDSVHandle();
	context.commandQueue->BeginRenderPass({ albedoTarget, normalTarget, materialTarget }, 0, &dsvHandle, true, false);
	context.commandQueue->SetViewports(1, &frameDesc.viewport);
	context.commandQueue->SetScissorRects(1, &frameDesc.scissorRect);

	D3D12_GPU_VIRTUAL_ADDRESS cameraGpuAddress = 0;
	const JCameraResource* cameraResource = context.renderDB->FindCameraResource(frameDesc.camera);
	if (cameraResource != nullptr)
	{
		cameraGpuAddress = context.commandQueue->UploadFrameConstantBuffer(&cameraResource->constants, sizeof(cameraResource->constants));
	}

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

		const JMeshResource* meshResource = resources.mesh;
		if (!meshResource->hasNormals || !meshResource->hasTexcoords)
		{
			++_lastStats.skippedDrawCount;
			continue;
		}

		Render::JGraphicResource graphicResource(_shader);
		if (!buildDrawGraphicResource(context, drawItem, graphicResource, cameraGpuAddress))
		{
			++_lastStats.skippedDrawCount;
			continue;
		}

		context.commandQueue->SetPipeline(_pipeline);
		context.commandQueue->SetGraphicResources(&graphicResource);
		context.commandQueue->BindVertexBuffer(meshResource);
		const uint32 indexCount = drawItem.indexCount != 0 ? drawItem.indexCount : static_cast<uint32>(meshResource->indexSize);
		context.commandQueue->DrawIndexed(indexCount, 1, drawItem.startIndex, 0, 0);
		++_lastStats.drawCallCount;
	}

	context.commandQueue->EndRenderPass();
	context.commandQueue->TransitionRenderTarget(albedoTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.commandQueue->TransitionRenderTarget(normalTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.commandQueue->TransitionRenderTarget(materialTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

J_ENGINE_END
