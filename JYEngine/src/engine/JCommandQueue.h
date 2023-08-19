#pragma once

#ifndef __J_COMMAND_QUEUE_H__
#define __J_COMMAND_QUEUE_H__
#include "engine/precompile.h"

/*#include "engine/JSwapChain.h"*/ namespace J { namespace Engine { class JSwapChain; } }
/*#include "engine/JDescriptorHeap.h"*/ namespace J { namespace Engine { class JDescriptorHeap; } }

J_ENGINE_BEGINE

class JCommandQueue
{
public:
	JCommandQueue(ComPtr<ID3D12Device> device, JSwapChain* swapChain, JDescriptorHeap* descriptorHeap);
	~JCommandQueue();

	


private:

	void initialize(ComPtr<ID3D12Device> device, JSwapChain* swapChain, JDescriptorHeap* descriptorHeap);
	void destroy();

	void waitSync();

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

	const JSwapChain*			_swapChain;
	const JDescriptorHeap*	_descriptorHeap;
	
};

J_ENGINE_END

#endif