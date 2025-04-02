#pragma once

#ifndef __J_COMMAND_QUEUE_H__
#define __J_COMMAND_QUEUE_H__
#include "engine/precompile.h"

/*#include "engine/JSwapChain.h"*/ namespace J { namespace Render { class JSwapChain; } }
/*#include "engine/JDescriptorHeap.h"*/ namespace J { namespace Render { class JDescriptorHeap; } }
/*#include "engine/JRenderTarget.h"*/ namespace J { namespace Engine { class JRenderTarget; } }
/*#inlcude "enigne/JRenderDefinition.h*/ namespace J { namespace Render { struct JPipeline; } }
/*#include "engine/JRenderResource.h"*/ namespace J { namespace Engine { struct JMeshResource; } }
/*#include "engine/asset/JShader.h"*/ namespace J { namespace Render { class JShader; } }

J_RENDER_BEGIN

class JCommandQueue
{
public:
	JCommandQueue();
	~JCommandQueue();

	void Initialize(ComPtr<ID3D12Device> device, JSwapChain* swapChain);

	//TODO: Divide
	
	void RenderBegin();

	void BeginRenderPass(Engine::JRenderTarget* renderTarget, const JColor& clearColor, uint32 rectCount);
	void SetViewports(const uint32& viewPortCount, const D3D12_VIEWPORT* viewport);
	void SetScissorRects(const uint32& rectCount, const D3D12_RECT* rect);
	void SetPipeline(const JPipeline* pipeline);
	void SetGraphicResources(JShader* shader);
	void BindVertexBuffer(const Engine::JMeshResource* meshResource);
	void Draw(const uint32& vertexCount, const uint32& instanceCount);
	void DrawIndexed(const uint32& indexCount, const uint32& instanceCount, const uint32& startIndex, const uint32& baseVertex, const uint32& startInstance);
	
	void EndRenderPass();
	
	void RenderEnd();

	void WaitSync();

	ComPtr<ID3D12CommandQueue> GetCmdQueue() { return _cmdQueue; }
	ComPtr<ID3D12GraphicsCommandList> GetCmdList() { return	_cmdList; }

private:

	void destroy();
	

	// CommandQueue : DX12에 등장
	// 외주를 요청할 때, 하나씩 요청하면 비효율적
	// [외주 목록]에 일감을 차곡차곡 기록했다가 한 방에 요청하는 것
	ComPtr<ID3D12CommandQueue>			_cmdQueue;
	ComPtr<ID3D12CommandAllocator>		_cmdAlloc;
	ComPtr<ID3D12GraphicsCommandList>	_cmdList;

	// Fence : 울타리(?)
	// CPU / GPU 동기화를 위한 간단한 도구
	ComPtr<ID3D12Fence>					_fence;
	uint32								_fenceValue = 0;
	HANDLE								_fenceEvent = INVALID_HANDLE_VALUE;

	Engine::JRenderTarget* _currentRenderTarget = nullptr;
};

J_RENDER_END

#endif