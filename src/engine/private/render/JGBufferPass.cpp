#include "engine/render/JGBufferPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JGraphicResource.h"
#include "engine/render/JMaterialResource.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JRenderDB.h"
#include "engine/asset/JShader.h"

#include <iostream>

J_ENGINE_BEGIN

namespace
{
	bool buildDrawGraphicResource(const JRenderPassContext& context, JCameraHandle camera, const JDrawItem& drawItem, Render::JGraphicResource& graphicResource)
	{
		if (!context.renderDB->BuildGraphicResource(drawItem.materialID, graphicResource.GetShader(), graphicResource))
		{
			return false;
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
		_pipeline = context.renderContext->CreatePipeline(_shader, false, true, true, true, gBufferFormats);
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
	context.commandQueue->BeginRenderPass({ albedoTarget, normalTarget, materialTarget }, 0, &dsvHandle, true, true);
	context.commandQueue->SetViewports(1, &frameDesc.viewport);
	context.commandQueue->SetScissorRects(1, &frameDesc.scissorRect);

	for (const JDrawItem& drawItem : frameDesc.opaqueDrawItems)
	{
		if (drawItem.meshResource == nullptr)
		{
			++_lastStats.skippedDrawCount;
			continue;
		}

		const JMeshResource* meshResource = drawItem.meshResource;
		const JMaterialResource* materialResource = drawItem.materialResource;
		if (meshResource == nullptr || materialResource == nullptr || !meshResource->hasNormals || !meshResource->hasTexcoords)
		{
			++_lastStats.skippedDrawCount;
			continue;
		}

		Render::JGraphicResource graphicResource(_shader);
		if (!buildDrawGraphicResource(context, frameDesc.camera, drawItem, graphicResource))
		{
			++_lastStats.skippedDrawCount;
			continue;
		}

		context.commandQueue->SetPipeline(_pipeline);
		context.commandQueue->SetGraphicResources(&graphicResource);
		context.commandQueue->BindVertexBuffer(meshResource);
		context.commandQueue->DrawIndexed(static_cast<uint32>(meshResource->indexSize), 1, 0, 0, 0);
		++_lastStats.drawCallCount;
	}

	context.commandQueue->EndRenderPass();
	context.commandQueue->TransitionRenderTarget(albedoTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.commandQueue->TransitionRenderTarget(normalTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.commandQueue->TransitionRenderTarget(materialTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

J_ENGINE_END
