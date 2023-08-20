#pragma once
#ifndef __J_DESCRIPTOR_HEAP_H__
#define	__J_DESCRIPTOR_HEAP_H__

#include "engine/JRenderDefinition.h"

/*#include "engine/JSwapChain.h"*/ namespace J { namespace Render { class JSwapChain; } }

J_RENDER_BEGIN

// [기안서]
// 외주를 맡길 때 이런 저런 정보들을 같이 넘겨줘야 하는데,
// 아무 형태로나 요청하면 못 알아먹는다
// - 각종 리소스를 어떤 용도로 사용하는지 꼼꼼하게 적어서 넘겨주는 용도
class JDescriptorHeap // View
{
public:
	JDescriptorHeap();
	
	void Initialize(ComPtr<ID3D12Device> device, JSwapChain* swapChain);

	D3D12_CPU_DESCRIPTOR_HANDLE		GetRTV(int32 idx) { return _rtvHandle[idx]; }

	D3D12_CPU_DESCRIPTOR_HANDLE		GetBackBufferView();

private:
	ComPtr<ID3D12DescriptorHeap>	_rtvHeap;
	uint32							_rtvHeapSize = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE		_rtvHandle[SWAP_CHAIN_BUFFER_COUNT];

	const JSwapChain* _swapChain;
};

J_RENDER_END

#endif