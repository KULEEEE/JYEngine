#include "engine/render/JLightingPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JGraphicResource.h"
#include "engine/render/JRenderDB.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JRenderTarget.h"
#include "engine/asset/JShader.h"

#include <iostream>

J_ENGINE_BEGIN

JLightingPass::~JLightingPass()
{
	delete _pipeline;
	delete _shader;
	_pipeline = nullptr;
	_shader = nullptr;
}

bool JLightingPass::ensureResources(const JRenderPassContext& context)
{
	if (_shader != nullptr && _pipeline != nullptr)
	{
		return true;
	}

	if (context.renderContext == nullptr)
	{
		return false;
	}

	const std::string shaderPath = get_Engine_Shader_Path() + "\\lighting_copy.hlsl";
	_shader = context.renderContext->CreateShader(shaderPath);
	if (_shader == nullptr)
	{
		std::cerr << "JLightingPass failed: lighting shader creation failed." << std::endl;
		return false;
	}

	_pipeline = context.renderContext->CreatePipeline(_shader, false, false, false, false, {}, false);
	if (_pipeline == nullptr)
	{
		std::cerr << "JLightingPass failed: pipeline creation failed." << std::endl;
		return false;
	}

	return true;
}

void JLightingPass::Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc)
{
	_lastStats = {};
	_lastStats.name = GetName();

	if (context.commandQueue == nullptr || context.gBuffer == nullptr || !context.gBuffer->IsValid() || frameDesc.renderTarget == nullptr)
	{
		return;
	}

	JRenderTarget* albedoTarget = context.gBuffer->GetAlbedoTarget();
	JRenderTarget* normalTarget = context.gBuffer->GetNormalTarget();
	JRenderTarget* materialTarget = context.gBuffer->GetMaterialTarget();
	Render::JTexture* albedoTexture = albedoTarget != nullptr ? albedoTarget->GetTextureView() : nullptr;
	Render::JTexture* normalTexture = normalTarget != nullptr ? normalTarget->GetTextureView() : nullptr;
	Render::JTexture* materialTexture = materialTarget != nullptr ? materialTarget->GetTextureView() : nullptr;
	if (albedoTexture == nullptr || normalTexture == nullptr || materialTexture == nullptr || !ensureResources(context))
	{
		return;
	}

	context.commandQueue->BeginRenderPass(frameDesc.renderTarget, frameDesc.clearColor, 0);
	context.commandQueue->SetViewports(1, &frameDesc.viewport);
	context.commandQueue->SetScissorRects(1, &frameDesc.scissorRect);

	Render::JGraphicResource graphicResource(_shader);
	const JLightResource* lightResource = context.renderDB != nullptr ? context.renderDB->FindLightResource() : nullptr;
	if (lightResource != nullptr && lightResource->initialized)
	{
		const D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = context.commandQueue->UploadFrameConstantBuffer(&lightResource->constants, sizeof(lightResource->constants));
		graphicResource.SetConstantBufferAddress("PerLights", gpuAddress);
	}
	graphicResource.SetTexture("GBufferAlbedo", albedoTexture);
	graphicResource.SetTexture("GBufferNormal", normalTexture);
	graphicResource.SetTexture("GBufferMaterial", materialTexture);

	context.commandQueue->SetPipeline(_pipeline);
	context.commandQueue->SetGraphicResources(&graphicResource);
	context.commandQueue->DrawFullscreenTriangle();
	++_lastStats.drawCallCount;

	context.commandQueue->EndRenderPass();
}

J_ENGINE_END
