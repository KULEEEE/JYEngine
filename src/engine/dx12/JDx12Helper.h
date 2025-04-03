#pragma once
#ifndef __J_DX12_HELPER_H__
#define __J_DX12_HELPER_H__

#include "engine/precompile.h"

J_RENDER_BEGIN

class JDx12Helper
{
public:
	JDx12Helper() = delete;
	JDx12Helper(ComPtr<ID3D12Device> device);
	~JDx12Helper();

	D3D12_CPU_DESCRIPTOR_HANDLE CreateCPUDescriptorHandle(ID3D12Resource* rtvResource);
private:
	ComPtr<ID3D12Device> _device;
	ComPtr<ID3D12DescriptorHeap> _rtvHeap;

	D3D12_CPU_DESCRIPTOR_HANDLE _rtvHeapBegin;

	//TODO: 다른 Heap들도 추가

	int _rtvHeapSize = 0;
	uint16 _rtvIndex = 0;
};

J_RENDER_END
#endif