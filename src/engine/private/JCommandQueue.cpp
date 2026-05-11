#include "engine/JCommandQueue.h"
#include "engine/JEngineContext.h"
#include "engine/JSwapChain.h"
#include "engine/JRenderTarget.h"
#include "engine/JGraphicResource.h"
#include "engine/JRenderDefinition.h"
#include "engine/JRenderResource.h"
#include "engine/asset/JShader.h"

#include <iostream>

J_RENDER_BEGIN

using namespace J::Engine;

JCommandQueue::JCommandQueue()
{
}

JCommandQueue::~JCommandQueue()
{
}

void JCommandQueue::Initialize(ComPtr<ID3D12Device> device, JSwapChain* swapChain)
{
	if (device == nullptr)
	{
		std::cerr << "JCommandQueue::Initialize failed: device is null." << std::endl;
		return;
	}

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	HRESULT hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_cmdQueue));
	if (FAILED(hr))
	{
		std::cerr << "CreateCommandQueue failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return;
	}

	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAlloc));
	if (FAILED(hr))
	{
		std::cerr << "CreateCommandAllocator failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return;
	}

	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&_cmdList));
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

void JCommandQueue::RenderBegin()
{
	if (_cmdAlloc == nullptr || _cmdList == nullptr)
	{
		std::cerr << "RenderBegin skipped: command queue is not initialized." << std::endl;
		return;
	}

	_cmdAlloc->Reset();
	_cmdList->Reset(_cmdAlloc.Get(), nullptr);
}

void JCommandQueue::BeginRenderPass(Engine::JRenderTarget* renderTarget, const JColor& clearColor, uint32 rectCount)
{
	if (_cmdList == nullptr || renderTarget == nullptr)
	{
		std::cerr << "BeginRenderPass skipped: command list or render target is null." << std::endl;
		return;
	}

	_currentRenderTarget = renderTarget;

	vector<D3D12_RESOURCE_BARRIER> barriers;
	vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles = renderTarget->GetRTVHandle();

	for (auto& rtvResource : renderTarget->GetRTVResource())
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			rtvResource,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET));
	}

	_cmdList->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
	for (auto& rtvHandle : rtvHandles)
	{
		_cmdList->ClearRenderTargetView(rtvHandle, clearColor, rectCount, nullptr);
	}

	_cmdList->OMSetRenderTargets(static_cast<uint32>(rtvHandles.size()), rtvHandles.data(), FALSE, nullptr);
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
		if (binding.buffer == nullptr || binding.buffer->buffer == nullptr)
		{
			continue;
		}

		_cmdList->SetGraphicsRootConstantBufferView(binding.rootParameterIndex, binding.buffer->buffer->GetGPUVirtualAddress());
	}

	if (!resource->GetTextures().empty())
	{
		const JTexture* texture = resource->GetTextures().front().texture;
		if (texture != nullptr && texture->srvHeap != nullptr)
		{
			ID3D12DescriptorHeap* heaps[] = { texture->srvHeap, texture->samplerHeap };
			_cmdList->SetDescriptorHeaps(texture->samplerHeap != nullptr ? 2u : 1u, heaps);

			const uint32 textureRootParameterIndex = static_cast<uint32>(shader->bindingInfo.cBuffers.size());
			_cmdList->SetGraphicsRootDescriptorTable(textureRootParameterIndex, texture->srvHeap->GetGPUDescriptorHandleForHeapStart());

			if (texture->samplerHeap != nullptr && !shader->bindingInfo.samplers.empty())
			{
				const uint32 samplerRootParameterIndex = textureRootParameterIndex + 1;
				_cmdList->SetGraphicsRootDescriptorTable(samplerRootParameterIndex, texture->samplerHeap->GetGPUDescriptorHandleForHeapStart());
			}
		}
	}
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
	_cmdList->DrawInstanced(vertexCount, instanceCount, 0, 0);
}

void JCommandQueue::DrawIndexed(const uint32& indexCount, const uint32& instanceCount, const uint32& startIndex, const uint32& baseVertex, const uint32& startInstance)
{
	if (_cmdList == nullptr)
	{
		return;
	}
	_cmdList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void JCommandQueue::EndRenderPass()
{
	if (_cmdList == nullptr || _currentRenderTarget == nullptr)
	{
		std::cerr << "EndRenderPass skipped: command list or current render target is null." << std::endl;
		return;
	}

	vector<D3D12_RESOURCE_BARRIER> barriers;

	for (auto& rtvResource : _currentRenderTarget->GetRTVResource())
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			rtvResource,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT));
	}

	_cmdList->ResourceBarrier(static_cast<uint32>(barriers.size()), barriers.data());
}

void JCommandQueue::RenderEnd()
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
}

void JCommandQueue::WaitSync()
{
	if (_cmdQueue == nullptr || _fence == nullptr || _fenceEvent == nullptr)
	{
		std::cerr << "WaitSync skipped: synchronization objects are not initialized." << std::endl;
		return;
	}

	_fenceValue++;
	_cmdQueue->Signal(_fence.Get(), _fenceValue);

	if (_fence->GetCompletedValue() < _fenceValue)
	{
		_fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
		::WaitForSingleObject(_fenceEvent, INFINITE);
	}
}

void JCommandQueue::destroy()
{
}

J_RENDER_END
