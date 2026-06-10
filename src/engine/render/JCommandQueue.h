#pragma once

#ifndef __J_COMMAND_QUEUE_H__
#define __J_COMMAND_QUEUE_H__
#include "engine/precompile.h"
#include "engine/render/JRenderDefinition.h"

/*#include "engine/render/JSwapChain.h"*/ namespace J { namespace Render { class JSwapChain; } }
/*#include "engine/render/JDescriptorHeap.h"*/ namespace J { namespace Render { class JDescriptorHeap; } }
/*#include "engine/render/JRenderTarget.h"*/ namespace J { namespace Engine { class JRenderTarget; } }
/*#include "engine/render/JRenderResource.h"*/ namespace J { namespace Engine { struct JMeshResource; } }
/*#include "engine/asset/JShader.h"*/ namespace J { namespace Render { class JShader; } }
/*#include "engine/render/JGraphicResource.h"*/ namespace J { namespace Render { class JGraphicResource; } }

J_RENDER_BEGIN

class JCommandQueue
{
public:
	JCommandQueue();
	~JCommandQueue();

	void Initialize(ComPtr<ID3D12Device> device, JSwapChain* swapChain);

	void RenderBegin(uint32 frameIndex);

	void BeginRenderPass(Engine::JRenderTarget* renderTarget, const JColor& clearColor, uint32 rectCount, bool clearRenderTarget = true);
	void BeginRenderPass(Engine::JRenderTarget* renderTarget, const JColor& clearColor, uint32 rectCount, D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle, bool clearRenderTarget = true, bool clearDepth = false);
	void BeginRenderPass(const std::vector<Engine::JRenderTarget*>& renderTargets, uint32 rectCount, bool clearRenderTarget = true);
	void BeginRenderPass(const std::vector<Engine::JRenderTarget*>& renderTargets, uint32 rectCount, D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle, bool clearRenderTarget = true, bool clearDepth = false);
	void BeginDepthPass(D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle, uint32 rectCount, bool clearDepth = true);
	void TransitionRenderTarget(Engine::JRenderTarget* renderTarget, D3D12_RESOURCE_STATES targetState);
	void SetViewports(const uint32& viewPortCount, const D3D12_VIEWPORT* viewport);
	void SetScissorRects(const uint32& rectCount, const D3D12_RECT* rect);
	void SetPipeline(const JPipeline* pipeline);
	void SetGraphicResources(JShader* shader);
	void SetGraphicResources(const JGraphicResource* resource);
	void BindVertexBuffer(const Engine::JMeshResource* meshResource);
	void Draw(const uint32& vertexCount, const uint32& instanceCount);
	void DrawFullscreenTriangle();
	void DrawIndexed(const uint32& indexCount, const uint32& instanceCount, const uint32& startIndex, const uint32& baseVertex, const uint32& startInstance);
	D3D12_GPU_VIRTUAL_ADDRESS UploadFrameConstantBuffer(const void* data, size_t size);

	void EndRenderPass();
	void RenderEnd(uint32 frameIndex);
	void WaitIdle();

	ComPtr<ID3D12CommandQueue> GetCmdQueue() { return _cmdQueue; }
	ComPtr<ID3D12GraphicsCommandList> GetCmdList() { return _cmdList; }

private:
	struct FrameResource
	{
		ComPtr<ID3D12CommandAllocator> commandAllocator;
		ComPtr<ID3D12Resource> uploadBuffer;
		uint8* uploadMappedData = nullptr;
		size_t uploadCapacity = 0;
		size_t uploadOffset = 0;
		ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		uint32 descriptorCapacity = 0;
		uint32 descriptorOffset = 0;
		std::unordered_map<size_t, D3D12_GPU_DESCRIPTOR_HANDLE> descriptorTableCache;
		uint64 fenceValue = 0;
	};

	void destroy();
	void waitForFenceValue(uint64 fenceValue);
	bool initializeFrameResource(FrameResource& frameResource);
	D3D12_GPU_DESCRIPTOR_HANDLE allocateFrameTextureTable(const JGraphicResource* resource, JShader* shader);
	size_t makeTextureTableKey(const JGraphicResource* resource, JShader* shader) const;

	ComPtr<ID3D12CommandQueue> _cmdQueue;
	ComPtr<ID3D12Device> _device;
	ComPtr<ID3D12GraphicsCommandList> _cmdList;

	ComPtr<ID3D12Fence> _fence;
	uint64 _nextFenceValue = 1;
	HANDLE _fenceEvent = INVALID_HANDLE_VALUE;
	FrameResource _frameResources[SWAP_CHAIN_BUFFER_COUNT];
	uint32 _activeFrameIndex = 0;
	uint32 _srvDescriptorSize = 0;

	Engine::JRenderTarget* _currentRenderTarget = nullptr;
	std::vector<Engine::JRenderTarget*> _currentRenderTargets;
};

J_RENDER_END

#endif
