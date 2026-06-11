#include "engine/render/JDepthPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JDrawItemCache.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JRenderDB.h"
#include "engine/asset/JShader.h"
#include "engine/core/JHashFunction.h"

J_ENGINE_BEGIN

namespace
{
	uint32 findCBufferRootParameterIndex(Render::JShader* shader, const char* name)
	{
		if (shader == nullptr || name == nullptr)
		{
			return static_cast<uint32>(-1);
		}

		const uint32 nameHash = JHashFunction::StrCrc32(name);
		for (uint32 i = 0; i < static_cast<uint32>(shader->bindingInfo.cBuffers.size()); ++i)
		{
			if (shader->bindingInfo.cBuffers[i].nameHash == nameHash)
			{
				return i;
			}
		}
		return static_cast<uint32>(-1);
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
		if (_shader != nullptr)
		{
			_perObjectRootParameterIndex = findCBufferRootParameterIndex(_shader, "PerObject");
			_perFrameRootParameterIndex = findCBufferRootParameterIndex(_shader, "PerFrame");
		}
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
	context.commandQueue->SetPipeline(_pipeline);
	context.commandQueue->SetGraphicResources(_shader);

	D3D12_GPU_VIRTUAL_ADDRESS cameraGpuAddress = 0;
	const JCameraResource* cameraResource = context.renderDB->FindCameraResource(frameDesc.camera);
	if (cameraResource != nullptr)
	{
		cameraGpuAddress = context.commandQueue->UploadFrameConstantBuffer(&cameraResource->constants, sizeof(cameraResource->constants));
	}
	if (_perFrameRootParameterIndex != static_cast<uint32>(-1))
	{
		context.commandQueue->SetGraphicsRootConstantBufferView(_perFrameRootParameterIndex, cameraGpuAddress);
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

		const JTransformResource* transformResource = resources.transform;
		if (transformResource == nullptr)
		{
			++_lastStats.skippedDrawCount;
			continue;
		}

		if (_perObjectRootParameterIndex != static_cast<uint32>(-1))
		{
			const D3D12_GPU_VIRTUAL_ADDRESS objectGpuAddress = context.commandQueue->UploadFrameConstantBuffer(&transformResource->constants, sizeof(transformResource->constants));
			context.commandQueue->SetGraphicsRootConstantBufferView(_perObjectRootParameterIndex, objectGpuAddress);
		}
		context.commandQueue->BindVertexBuffer(resources.mesh);
		const uint32 indexCount = drawItem.indexCount != 0 ? drawItem.indexCount : static_cast<uint32>(resources.mesh->indexSize);
		context.commandQueue->DrawIndexed(indexCount, 1, drawItem.startIndex, 0, 0);
		++_lastStats.drawCallCount;
	}

	context.commandQueue->EndRenderPass();
}

J_ENGINE_END
