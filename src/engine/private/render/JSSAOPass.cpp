#include "engine/render/JSSAOPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JGraphicResource.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JRenderResource.h"
#include "engine/render/JRenderTarget.h"
#include "engine/asset/JShader.h"

#include <iostream>

J_ENGINE_BEGIN

JSSAOPass::~JSSAOPass()
{
	delete _pipeline;
	delete _shader;
	_pipeline = nullptr;
	_shader = nullptr;
}

bool JSSAOPass::ensureResources(const JRenderPassContext& context)
{
	if (_shader != nullptr && _pipeline != nullptr)
	{
		return true;
	}
	if (context.renderContext == nullptr)
	{
		return false;
	}

	const std::string shaderPath = get_Engine_Shader_Path() + "\\ssao.hlsl";
	_shader = context.renderContext->CreateShader(shaderPath);
	if (_shader == nullptr)
	{
		std::cerr << "JSSAOPass failed: shader creation failed: " << shaderPath << std::endl;
		return false;
	}

	// 단일 채널 R8 타겟, depth/vertex 입력 없음.
	_pipeline = context.renderContext->CreatePipeline(
		_shader, false, false, false, false,
		{ DXGI_FORMAT_R8_UNORM }, false,
		D3D12_DEPTH_WRITE_MASK_ALL, D3D12_COMPARISON_FUNC_GREATER, false);
	if (_pipeline == nullptr)
	{
		std::cerr << "JSSAOPass failed: pipeline creation failed." << std::endl;
		return false;
	}
	return true;
}

void JSSAOPass::Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc)
{
	_lastStats = {};
	_lastStats.name = GetName();

	// 캡처(probe)에서는 SSAO를 건너뛴다.
	if (frameDesc.captureMode)
	{
		return;
	}
	if (context.commandQueue == nullptr || context.gBuffer == nullptr || !context.gBuffer->IsValid() || context.ssaoTarget == nullptr)
	{
		return;
	}

	JRenderTarget* normalTarget = context.gBuffer->GetNormalTarget();
	JRenderTarget* depthTarget = context.gBuffer->GetDepthTarget();
	Render::JTexture* normalTexture = normalTarget != nullptr ? normalTarget->GetTextureView() : nullptr;
	Render::JTexture* depthTexture = depthTarget != nullptr ? depthTarget->GetTextureView() : nullptr;
	if (normalTexture == nullptr || depthTexture == nullptr || !ensureResources(context))
	{
		return;
	}

	// depth를 SRV로 읽기 위해 전환(LightingPass가 이어서 그대로 PSR로 사용).
	context.commandQueue->TransitionRenderTarget(depthTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(context.ssaoTarget->GetWidth()), static_cast<float>(context.ssaoTarget->GetHeight()), 0.0f, 1.0f };
	const D3D12_RECT scissor = { 0, 0, static_cast<LONG>(context.ssaoTarget->GetWidth()), static_cast<LONG>(context.ssaoTarget->GetHeight()) };

	JColor white;
	white.r = 1.0f; white.g = 1.0f; white.b = 1.0f; white.a = 1.0f;
	context.commandQueue->BeginRenderPass(context.ssaoTarget, white, 0);
	context.commandQueue->SetViewports(1, &viewport);
	context.commandQueue->SetScissorRects(1, &scissor);

	Render::JGraphicResource graphicResource(_shader);
	JPerViewConstants viewConstants{};
	XMStoreFloat4x4(&viewConstants.inverseViewProjection, XMMatrixTranspose(frameDesc.cameraInverseViewProjection));
	viewConstants.cameraPosition = frameDesc.cameraWorldPosition;
	const D3D12_GPU_VIRTUAL_ADDRESS viewGpuAddress = context.commandQueue->UploadFrameConstantBuffer(&viewConstants, sizeof(viewConstants));
	graphicResource.SetConstantBufferAddress("PerView", viewGpuAddress);
	graphicResource.SetTexture("GBufferNormal", normalTexture);
	graphicResource.SetTexture("DepthBuffer", depthTexture);

	context.commandQueue->SetPipeline(_pipeline);
	context.commandQueue->SetGraphicResources(&graphicResource);
	context.commandQueue->DrawFullscreenTriangle();
	++_lastStats.drawCallCount;

	context.commandQueue->EndRenderPass();

	// LightingPass가 블러해서 읽도록 SRV 상태로 전환.
	context.commandQueue->TransitionRenderTarget(context.ssaoTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

J_ENGINE_END
