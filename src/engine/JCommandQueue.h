#pragma once

#ifndef __J_COMMAND_QUEUE_H__
#define __J_COMMAND_QUEUE_H__
#include "engine/precompile.h"

/*#include "engine/JSwapChain.h"*/ namespace J { namespace Render { class JSwapChain; } }
/*#include "engine/JDescriptorHeap.h"*/ namespace J { namespace Render { class JDescriptorHeap; } }
/*#include "engine/JRenderTarget.h"*/ namespace J { namespace Engine { class JRenderTarget; } }
/*#include "engine/JRenderDefinition.h"*/ namespace J { namespace Render { struct JPipeline; } }
/*#include "engine/JRenderResource.h"*/ namespace J { namespace Engine { struct JMeshResource; } }
/*#include "engine/asset/JShader.h"*/ namespace J { namespace Render { class JShader; } }
/*#include "engine/JGraphicResource.h"*/ namespace J { namespace Render { class JGraphicResource; } }

J_RENDER_BEGIN

class JCommandQueue
{
public:
	JCommandQueue();
	~JCommandQueue();

	void Initialize(ComPtr<ID3D12Device> device, JSwapChain* swapChain);

	void RenderBegin();

	void BeginRenderPass(Engine::JRenderTarget* renderTarget, const JColor& clearColor, uint32 rectCount);
	void SetViewports(const uint32& viewPortCount, const D3D12_VIEWPORT* viewport);
	void SetScissorRects(const uint32& rectCount, const D3D12_RECT* rect);
	void SetPipeline(const JPipeline* pipeline);
	void SetGraphicResources(JShader* shader);
	void SetGraphicResources(const JGraphicResource* resource);
	void BindVertexBuffer(const Engine::JMeshResource* meshResource);
	void Draw(const uint32& vertexCount, const uint32& instanceCount);
	void DrawIndexed(const uint32& indexCount, const uint32& instanceCount, const uint32& startIndex, const uint32& baseVertex, const uint32& startInstance);

	void EndRenderPass();
	void RenderEnd();
	void WaitSync();

	ComPtr<ID3D12CommandQueue> GetCmdQueue() { return _cmdQueue; }
	ComPtr<ID3D12GraphicsCommandList> GetCmdList() { return _cmdList; }

private:
	void destroy();

	ComPtr<ID3D12CommandQueue> _cmdQueue;
	ComPtr<ID3D12CommandAllocator> _cmdAlloc;
	ComPtr<ID3D12GraphicsCommandList> _cmdList;

	ComPtr<ID3D12Fence> _fence;
	uint32 _fenceValue = 0;
	HANDLE _fenceEvent = INVALID_HANDLE_VALUE;

	Engine::JRenderTarget* _currentRenderTarget = nullptr;
};

J_RENDER_END

#endif
