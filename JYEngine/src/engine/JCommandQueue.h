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

	// CommandQueue : DX12�� ����
	// ���ָ� ��û�� ��, �ϳ��� ��û�ϸ� ��ȿ����
	// [���� ���]�� �ϰ��� �������� ����ߴٰ� �� �濡 ��û�ϴ� ��
	ComPtr<ID3D12CommandQueue>			_cmdQueue;
	ComPtr<ID3D12CommandAllocator>		_cmdAlloc;
	ComPtr<ID3D12GraphicsCommandList>	_cmdList;

	// Fence : ��Ÿ��(?)
	// CPU / GPU ����ȭ�� ���� ������ ����
	ComPtr<ID3D12Fence>					_fence;
	uint32								_fenceValue = 0;
	HANDLE								_fenceEvent = INVALID_HANDLE_VALUE;

	const JSwapChain*			_swapChain;
	const JDescriptorHeap*	_descriptorHeap;
	
};

J_ENGINE_END

#endif