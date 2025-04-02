#include "engine/JCommandQueue.h"
#include "engine/JEngineContext.h"
#include "engine/JSwapChain.h"
#include "engine/JRenderTarget.h"
#include "engine/JRenderDefinition.h"
#include "engine/JRenderResource.h"
#include "engine/asset/JShader.h"

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
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_cmdQueue));

	// - D3D12_COMMAND_LIST_TYPE_DIRECT : GPU가 직접 실행하는 명령 목록
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAlloc));

	// GPU가 하나인 시스템에서는 0으로
	// DIRECT or BUNDLE
	// Allocator
	// 초기 상태 (그리기 명령은 nullptr 지정)
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&_cmdList));

	// CommandList는 Close / Open 상태가 있는데
	// Open 상태에서 Command를 넣다가 Close한 다음 제출하는 개념
	_cmdList->Close();

	// CreateFence
	// - CPU와 GPU의 동기화 수단으로 쓰인다
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
	_fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void JCommandQueue::RenderBegin()
{
	_cmdAlloc->Reset();
	_cmdList->Reset(_cmdAlloc.Get(), nullptr);
}

void JCommandQueue::BeginRenderPass(Engine::JRenderTarget* renderTarget, const JColor& clearColor, uint32 rectCount)
{
	_currentRenderTarget = renderTarget;

	vector<D3D12_RESOURCE_BARRIER> barriers;
	vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles = renderTarget->GetRTVHandle();

	for (auto& rtvResource : renderTarget->GetRTVResource())
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			rtvResource,
			D3D12_RESOURCE_STATE_PRESENT, // 화면 출력
			D3D12_RESOURCE_STATE_RENDER_TARGET)); // 외주 결과물
	}
	
	_cmdList->ResourceBarrier(barriers.size(), barriers.data());
	// ClearColors
	for (auto& rtvHandle : rtvHandles)
	{
		_cmdList->ClearRenderTargetView(rtvHandle, clearColor, rectCount, nullptr);
	}

	_cmdList->OMSetRenderTargets(rtvHandles.size(), rtvHandles.data(), FALSE, nullptr);
}

void JCommandQueue::SetViewports(const uint32& viewPortCount, const D3D12_VIEWPORT* viewport)
{
	_cmdList->RSSetViewports(viewPortCount, viewport);
}

void JCommandQueue::SetScissorRects(const uint32& rectCount, const D3D12_RECT* rect)
{
	_cmdList->RSSetScissorRects(rectCount, rect);
}

void JCommandQueue::SetPipeline(const JPipeline* pipeline)
{
	_cmdList->SetPipelineState(pipeline->pipelineState.Get());
}

void JCommandQueue::SetGraphicResources(JShader* shader)
{
	_cmdList->SetGraphicsRootSignature(shader->GetRootSignature()->signature.Get());
}

void JCommandQueue::BindVertexBuffer(const Engine::JMeshResource* meshResource)
{
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmdList->IASetVertexBuffers(0, meshResource->soaBuffers.size(), meshResource->soaBuffers.data());
	_cmdList->IASetIndexBuffer(&meshResource->indexBuffer);
}

void JCommandQueue::Draw(const uint32& vertexCount, const uint32& instanceCount)
{
	_cmdList->DrawInstanced(vertexCount, instanceCount, 0, 0);
}

void JCommandQueue::DrawIndexed(const uint32& indexCount, const uint32& instanceCount, const uint32& startIndex, const uint32& baseVertex, const uint32& startInstance)
{
	_cmdList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
}

void JCommandQueue::EndRenderPass()
{
	vector<D3D12_RESOURCE_BARRIER> barriers;


	for (auto& rtvResource : _currentRenderTarget->GetRTVResource())
	{
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			rtvResource,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT));
	}

	_cmdList->ResourceBarrier(barriers.size(), barriers.data());
}

void JCommandQueue::RenderEnd()
{
	_cmdList->Close();

	// 커맨드 리스트 수행
	ID3D12CommandList* cmdListArr[] = { _cmdList.Get() };
	_cmdQueue->ExecuteCommandLists(_countof(cmdListArr), cmdListArr);
}

void JCommandQueue::WaitSync()
{
	// Advance the fence value to mark commands up to this fence point.
	_fenceValue++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	_cmdQueue->Signal(_fence.Get(), _fenceValue);

	// Wait until the GPU has completed commands up to this fence point.
	if (_fence->GetCompletedValue() < _fenceValue)
	{
		// Fire event when GPU hits current fence.  
		_fence->SetEventOnCompletion(_fenceValue, _fenceEvent);

		// Wait until the GPU hits current fence event is fired.
		::WaitForSingleObject(_fenceEvent, INFINITE);
	}
}

void JCommandQueue::destroy()
{

}





J_RENDER_END