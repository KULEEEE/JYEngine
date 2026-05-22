#include "engine/render/JCommandQueue.h"
#include "engine/core/JEngineContext.h"
#include "engine/render/JSwapChain.h"
#include "engine/render/JRenderTarget.h"
#include "engine/render/JGraphicResource.h"
#include "engine/render/JRenderDefinition.h"
#include "engine/render/JRenderResource.h"
#include "engine/asset/JShader.h"

#include <iostream>

J_RENDER_BEGIN

using namespace J::Engine;

namespace
{
	constexpr size_t FRAME_UPLOAD_BUFFER_SIZE = 16 * 1024 * 1024;

	size_t alignTo(size_t value, size_t alignment)
	{
		return (value + alignment - 1) & ~(alignment - 1);
	}
}

JCommandQueue::JCommandQueue()
{
}

JCommandQueue::~JCommandQueue()
{
	destroy();
}

void JCommandQueue::Initialize(ComPtr<ID3D12Device> device, JSwapChain* swapChain)
{
	if (device == nullptr)
	{
		std::cerr << "JCommandQueue::Initialize failed: device is null." << std::endl;
		return;
	}

	_device = device;

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_cmdQueue));
	if (FAILED(hr))
	{
		std::cerr << "CreateCommandQueue failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return;
	}

	_srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (FrameResource& frameResource : _frameResources)
	{
		if (!initializeFrameResource(frameResource))
		{
			return;
		}
	}

	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _frameResources[0].commandAllocator.Get(), nullptr, IID_PPV_ARGS(&_cmdList));
	if (FAILED(hr))
	{
		std::cerr << "CreateCommandList failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return;
	}

	hr = _cmdList->Close();
	if (FAILED(hr))
	{
		std::cerr << "Initial command list Close failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return;
	}

	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
	if (FAILED(hr))
	{
		std::cerr << "CreateFence failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return;
	}
	_fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (_fenceEvent == nullptr)
	{
		std::cerr << "CreateEvent failed." << std::endl;
	}
}

bool JCommandQueue::initializeFrameResource(FrameResource& frameResource)
{
	HRESULT hr = _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frameResource.commandAllocator));
	if (FAILED(hr))
	{
		std::cerr << "CreateCommandAllocator failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return false;
	}

	const CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
	const CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(FRAME_UPLOAD_BUFFER_SIZE);
	hr = _device->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&uploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&frameResource.uploadBuffer));
	if (FAILED(hr))
	{
		std::cerr << "Create frame upload buffer failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return false;
	}

	hr = frameResource.uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&frameResource.uploadMappedData));
	if (FAILED(hr) || frameResource.uploadMappedData == nullptr)
	{
		std::cerr << "Map frame upload buffer failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return false;
	}
	frameResource.uploadCapacity = FRAME_UPLOAD_BUFFER_SIZE;
	frameResource.uploadOffset = 0;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = MAX_DESCRIPTOR_COUNT;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	hr = _device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&frameResource.descriptorHeap));
	if (FAILED(hr))
	{
		std::cerr << "Create frame descriptor heap failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return false;
	}
	frameResource.descriptorCapacity = MAX_DESCRIPTOR_COUNT;
	frameResource.descriptorOffset = 0;
	return true;
}

void JCommandQueue::RenderBegin(uint32 frameIndex)
{
	if (_cmdList == nullptr)
	{
		std::cerr << "RenderBegin skipped: command queue is not initialized." << std::endl;
		return;
	}

	_activeFrameIndex = frameIndex % SWAP_CHAIN_BUFFER_COUNT;
	FrameResource& frameResource = _frameResources[_activeFrameIndex];
	if (frameResource.commandAllocator == nullptr)
	{
		std::cerr << "RenderBegin skipped: frame command allocator is null." << std::endl;
		return;
	}

	waitForFenceValue(frameResource.fenceValue);
	frameResource.uploadOffset = 0;
	frameResource.descriptorOffset = 0;
	frameResource.commandAllocator->Reset();
	_cmdList->Reset(frameResource.commandAllocator.Get(), nullptr);
}

void JCommandQueue::BeginRenderPass(Engine::JRenderTarget* renderTarget, const JColor& clearColor, uint32 rectCount, bool clearRenderTarget)
{
	BeginRenderPass(renderTarget, clearColor, rectCount, nullptr, clearRenderTarget, false);
}

void JCommandQueue::BeginRenderPass(Engine::JRenderTarget* renderTarget, const JColor& clearColor, uint32 rectCount, D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle, bool clearRenderTarget, bool clearDepth)
{
	if (_cmdList == nullptr || renderTarget == nullptr)
	{
		std::cerr << "BeginRenderPass skipped: command list or render target is null." << std::endl;
		return;
	}

	_currentRenderTarget = renderTarget;
	_currentRenderTargets.clear();
	_currentRenderTargets.push_back(renderTarget);

	vector<D3D12_RESOURCE_BARRIER> barriers;
	vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles = renderTarget->GetRTVHandle();

	for (auto& rtvResource : renderTarget->GetRTVResource())
	{
		if (rtvResource != nullptr && renderTarget->GetResourceState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				rtvResource,
				renderTarget->GetResourceState(),
				D3D12_RESOURCE_STATE_RENDER_TARGET));
		}
	}

	if (!barriers.empty())
	{
		_cmdList->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
	}
	renderTarget->SetResourceState(D3D12_RESOURCE_STATE_RENDER_TARGET);

	if (clearRenderTarget)
	{
		for (auto& rtvHandle : rtvHandles)
		{
			_cmdList->ClearRenderTargetView(rtvHandle, clearColor, rectCount, nullptr);
		}
	}
	if (dsvHandle != nullptr && clearDepth)
	{
		_cmdList->ClearDepthStencilView(*dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, rectCount, nullptr);
	}

	_cmdList->OMSetRenderTargets(static_cast<uint32>(rtvHandles.size()), rtvHandles.data(), FALSE, dsvHandle);
}

void JCommandQueue::BeginRenderPass(const std::vector<Engine::JRenderTarget*>& renderTargets, uint32 rectCount, bool clearRenderTarget)
{
	BeginRenderPass(renderTargets, rectCount, nullptr, clearRenderTarget, false);
}

void JCommandQueue::BeginRenderPass(const std::vector<Engine::JRenderTarget*>& renderTargets, uint32 rectCount, D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle, bool clearRenderTarget, bool clearDepth)
{
	if (_cmdList == nullptr || renderTargets.empty())
	{
		std::cerr << "BeginRenderPass skipped: command list or render targets are invalid." << std::endl;
		return;
	}

	_currentRenderTarget = renderTargets.front();
	_currentRenderTargets = renderTargets;

	vector<D3D12_RESOURCE_BARRIER> barriers;
	vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;

	for (Engine::JRenderTarget* renderTarget : renderTargets)
	{
		if (renderTarget == nullptr || !renderTarget->IsValid())
		{
			continue;
		}

		for (ID3D12Resource* rtvResource : renderTarget->GetRTVResource())
		{
			if (rtvResource != nullptr && renderTarget->GetResourceState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
			{
				barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
					rtvResource,
					renderTarget->GetResourceState(),
					D3D12_RESOURCE_STATE_RENDER_TARGET));
			}
		}

		renderTarget->SetResourceState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		const auto& handles = renderTarget->GetRTVHandle();
		rtvHandles.insert(rtvHandles.end(), handles.begin(), handles.end());
	}

	if (rtvHandles.empty())
	{
		return;
	}

	if (!barriers.empty())
	{
		_cmdList->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
	}

	if (clearRenderTarget)
	{
		for (Engine::JRenderTarget* renderTarget : renderTargets)
		{
			if (renderTarget == nullptr)
			{
				continue;
			}

			for (const D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle : renderTarget->GetRTVHandle())
			{
				_cmdList->ClearRenderTargetView(rtvHandle, renderTarget->GetClearColor(), rectCount, nullptr);
			}
		}
	}
	if (dsvHandle != nullptr && clearDepth)
	{
		_cmdList->ClearDepthStencilView(*dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, rectCount, nullptr);
	}

	_cmdList->OMSetRenderTargets(static_cast<uint32>(rtvHandles.size()), rtvHandles.data(), FALSE, dsvHandle);
}

void JCommandQueue::TransitionRenderTarget(Engine::JRenderTarget* renderTarget, D3D12_RESOURCE_STATES targetState)
{
	if (_cmdList == nullptr || renderTarget == nullptr || renderTarget->GetResourceState() == targetState)
	{
		return;
	}

	vector<D3D12_RESOURCE_BARRIER> barriers;
	for (auto& rtvResource : renderTarget->GetRTVResource())
	{
		if (rtvResource == nullptr)
		{
			continue;
		}

		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			rtvResource,
			renderTarget->GetResourceState(),
			targetState));
	}

	if (!barriers.empty())
	{
		_cmdList->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
		renderTarget->SetResourceState(targetState);
	}
}

void JCommandQueue::SetViewports(const uint32& viewPortCount, const D3D12_VIEWPORT* viewport)
{
	if (_cmdList == nullptr)
	{
		return;
	}
	_cmdList->RSSetViewports(viewPortCount, viewport);
}

void JCommandQueue::SetScissorRects(const uint32& rectCount, const D3D12_RECT* rect)
{
	if (_cmdList == nullptr)
	{
		return;
	}
	_cmdList->RSSetScissorRects(rectCount, rect);
}

void JCommandQueue::SetPipeline(const JPipeline* pipeline)
{
	if (pipeline == nullptr)
	{
		std::cerr << "SetPipeline skipped: pipeline is null." << std::endl;
		return;
	}
	_cmdList->SetPipelineState(pipeline->pipelineState.Get());
}

void JCommandQueue::SetGraphicResources(JShader* shader)
{
	if (shader == nullptr || shader->GetRootSignature() == nullptr)
	{
		std::cerr << "SetGraphicResources skipped: shader or root signature is null." << std::endl;
		return;
	}
	_cmdList->SetGraphicsRootSignature(shader->GetRootSignature()->signature.Get());
}

void JCommandQueue::SetGraphicResources(const JGraphicResource* resource)
{
	if (resource == nullptr)
	{
		std::cerr << "SetGraphicResources skipped: graphic resource is null." << std::endl;
		return;
	}

	JShader* shader = resource->GetShader();
	if (shader == nullptr || shader->GetRootSignature() == nullptr)
	{
		std::cerr << "SetGraphicResources skipped: graphic resource shader is null." << std::endl;
		return;
	}

	SetGraphicResources(shader);

	for (const JGraphicResource::ConstantBufferBinding& binding : resource->GetConstantBuffers())
	{
		if (binding.gpuAddress != 0)
		{
			_cmdList->SetGraphicsRootConstantBufferView(binding.rootParameterIndex, binding.gpuAddress);
			continue;
		}

		if (binding.buffer == nullptr || binding.buffer->buffer == nullptr)
		{
			continue;
		}

		_cmdList->SetGraphicsRootConstantBufferView(binding.rootParameterIndex, binding.buffer->buffer->GetGPUVirtualAddress());
	}

	if (!resource->GetTextures().empty())
	{
		const D3D12_GPU_DESCRIPTOR_HANDLE tableHandle = allocateFrameTextureTable(resource, shader);
		if (tableHandle.ptr != 0)
		{
			const uint32 textureRootParameterIndex = static_cast<uint32>(shader->bindingInfo.cBuffers.size());
			_cmdList->SetGraphicsRootDescriptorTable(textureRootParameterIndex, tableHandle);
		}
	}
}

D3D12_GPU_DESCRIPTOR_HANDLE JCommandQueue::allocateFrameTextureTable(const JGraphicResource* resource, JShader* shader)
{
	D3D12_GPU_DESCRIPTOR_HANDLE emptyHandle{};
	if (_device == nullptr || resource == nullptr || shader == nullptr || _activeFrameIndex >= SWAP_CHAIN_BUFFER_COUNT)
	{
		return emptyHandle;
	}

	uint32 srvDescriptorCount = 0;
	for (const JShader::BindingInfo::Resource& textureBinding : shader->bindingInfo.textures)
	{
		srvDescriptorCount = std::max(srvDescriptorCount, textureBinding.slot + 1);
	}
	if (srvDescriptorCount == 0)
	{
		return emptyHandle;
	}

	FrameResource& frameResource = _frameResources[_activeFrameIndex];
	if (frameResource.descriptorHeap == nullptr || frameResource.descriptorOffset + srvDescriptorCount > frameResource.descriptorCapacity)
	{
		std::cerr << "Frame descriptor ring overflow: requested " << srvDescriptorCount << " descriptors." << std::endl;
		return emptyHandle;
	}

	const uint32 tableBase = frameResource.descriptorOffset;
	frameResource.descriptorOffset += srvDescriptorCount;

	for (const JGraphicResource::TextureBinding& binding : resource->GetTextures())
	{
		const JTexture* texture = binding.texture;
		if (texture == nullptr || texture->srvHeap == nullptr || binding.shaderSlot >= srvDescriptorCount)
		{
			continue;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE dest = frameResource.descriptorHeap->GetCPUDescriptorHandleForHeapStart();
		dest.ptr += static_cast<SIZE_T>(tableBase + binding.shaderSlot) * _srvDescriptorSize;
		_device->CopyDescriptorsSimple(1, dest, texture->srvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	ID3D12DescriptorHeap* heaps[] = { frameResource.descriptorHeap.Get() };
	_cmdList->SetDescriptorHeaps(1, heaps);

	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = frameResource.descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	gpuHandle.ptr += static_cast<UINT64>(tableBase) * _srvDescriptorSize;
	return gpuHandle;
}

void JCommandQueue::BindVertexBuffer(const Engine::JMeshResource* meshResource)
{
	if (_cmdList == nullptr || meshResource == nullptr)
	{
		std::cerr << "BindVertexBuffer skipped: command list or mesh resource is null." << std::endl;
		return;
	}
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmdList->IASetVertexBuffers(0, static_cast<uint32>(meshResource->soaBuffers.size()), meshResource->soaBuffers.data());
	_cmdList->IASetIndexBuffer(&meshResource->indexBuffer);
}

void JCommandQueue::Draw(const uint32& vertexCount, const uint32& instanceCount)
{
	if (_cmdList == nullptr)
	{
		return;
	}
	_cmdList->DrawInstanced(vertexCount, instanceCount, 0, 0);
}

void JCommandQueue::DrawFullscreenTriangle()
{
	if (_cmdList == nullptr)
	{
		return;
	}

	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmdList->DrawInstanced(3, 1, 0, 0);
}

void JCommandQueue::DrawIndexed(const uint32& indexCount, const uint32& instanceCount, const uint32& startIndex, const uint32& baseVertex, const uint32& startInstance)
{
	if (_cmdList == nullptr)
	{
		return;
	}
	_cmdList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

D3D12_GPU_VIRTUAL_ADDRESS JCommandQueue::UploadFrameConstantBuffer(const void* data, size_t size)
{
	if (data == nullptr || size == 0 || _activeFrameIndex >= SWAP_CHAIN_BUFFER_COUNT)
	{
		return 0;
	}

	FrameResource& frameResource = _frameResources[_activeFrameIndex];
	if (frameResource.uploadBuffer == nullptr || frameResource.uploadMappedData == nullptr)
	{
		return 0;
	}

	const size_t alignedOffset = alignTo(frameResource.uploadOffset, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	const size_t alignedSize = alignTo(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	if (alignedOffset + alignedSize > frameResource.uploadCapacity)
	{
		std::cerr << "Frame upload ring overflow: requested " << alignedSize << " bytes." << std::endl;
		return 0;
	}

	memcpy(frameResource.uploadMappedData + alignedOffset, data, size);
	frameResource.uploadOffset = alignedOffset + alignedSize;
	return frameResource.uploadBuffer->GetGPUVirtualAddress() + alignedOffset;
}

void JCommandQueue::EndRenderPass()
{
	if (_cmdList == nullptr || _currentRenderTarget == nullptr)
	{
		std::cerr << "EndRenderPass skipped: command list or current render target is null." << std::endl;
		return;
	}

	vector<D3D12_RESOURCE_BARRIER> barriers;

	for (Engine::JRenderTarget* renderTarget : _currentRenderTargets)
	{
		if (renderTarget == nullptr || !renderTarget->IsSwapChainTarget())
		{
			continue;
		}

		for (auto& rtvResource : renderTarget->GetRTVResource())
		{
			if (rtvResource != nullptr)
			{
				barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
					rtvResource,
					renderTarget->GetResourceState(),
					D3D12_RESOURCE_STATE_PRESENT));
			}
		}
		renderTarget->SetResourceState(D3D12_RESOURCE_STATE_PRESENT);
	}

	if (!barriers.empty())
	{
		_cmdList->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
	}
}

void JCommandQueue::RenderEnd(uint32 frameIndex)
{
	if (_cmdList == nullptr || _cmdQueue == nullptr)
	{
		std::cerr << "RenderEnd skipped: command queue is not initialized." << std::endl;
		return;
	}

	HRESULT hr = _cmdList->Close();
	if (FAILED(hr))
	{
		std::cerr << "RenderEnd Close failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return;
	}

	ID3D12CommandList* cmdListArr[] = { _cmdList.Get() };
	_cmdQueue->ExecuteCommandLists(_countof(cmdListArr), cmdListArr);

	const uint32 resolvedFrameIndex = frameIndex % SWAP_CHAIN_BUFFER_COUNT;
	const uint64 fenceValue = _nextFenceValue++;
	_cmdQueue->Signal(_fence.Get(), fenceValue);
	_frameResources[resolvedFrameIndex].fenceValue = fenceValue;
}

void JCommandQueue::waitForFenceValue(uint64 fenceValue)
{
	if (fenceValue == 0 || _fence == nullptr || _fenceEvent == nullptr)
	{
		return;
	}

	if (_fence->GetCompletedValue() < fenceValue)
	{
		_fence->SetEventOnCompletion(fenceValue, _fenceEvent);
		::WaitForSingleObject(_fenceEvent, INFINITE);
	}
}

void JCommandQueue::WaitIdle()
{
	if (_cmdQueue == nullptr || _fence == nullptr || _fenceEvent == nullptr)
	{
		return;
	}

	const uint64 fenceValue = _nextFenceValue++;
	_cmdQueue->Signal(_fence.Get(), fenceValue);
	waitForFenceValue(fenceValue);

	for (FrameResource& frameResource : _frameResources)
	{
		frameResource.fenceValue = 0;
	}
}

void JCommandQueue::destroy()
{
	WaitIdle();

	if (_fenceEvent != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(_fenceEvent);
		_fenceEvent = INVALID_HANDLE_VALUE;
	}

	_currentRenderTarget = nullptr;
	_currentRenderTargets.clear();
	_cmdList.Reset();
	for (FrameResource& frameResource : _frameResources)
	{
		if (frameResource.uploadBuffer != nullptr && frameResource.uploadMappedData != nullptr)
		{
			frameResource.uploadBuffer->Unmap(0, nullptr);
			frameResource.uploadMappedData = nullptr;
		}
		frameResource.uploadBuffer.Reset();
		frameResource.uploadCapacity = 0;
		frameResource.uploadOffset = 0;
		frameResource.descriptorHeap.Reset();
		frameResource.descriptorCapacity = 0;
		frameResource.descriptorOffset = 0;
		frameResource.commandAllocator.Reset();
		frameResource.fenceValue = 0;
	}
	_cmdQueue.Reset();
	_fence.Reset();
	_device.Reset();
}

J_RENDER_END
