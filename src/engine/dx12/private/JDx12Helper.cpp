#include "engine/dx12/jdx12Helper.h"
#include "engine/JRenderDefinition.h"

J_RENDER_BEGIN

JDx12Helper::JDx12Helper(ComPtr<ID3D12Device> device)
{
	_device = device;

	D3D12_DESCRIPTOR_HEAP_DESC rtvDesc;
	rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDesc.NumDescriptors = MAX_DESCRIPTOR_COUNT;
	rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDesc.NodeMask = 0;

	// 같은 종류의 데이터끼리 배열로 관리
	// RTV 목록 : [ ] [ ]
	device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&_rtvHeap));

	_rtvHeapSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_rtvHeapBegin = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
}

const D3D12_CPU_DESCRIPTOR_HANDLE& JDx12Helper::CreateCPUDescriptorHandle(ID3D12Resource* rtvResource)
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
	rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(_rtvHeapBegin,  _rtvIndex*_rtvHeapSize);
	_device->CreateRenderTargetView(rtvResource, nullptr, rtvHandle);
	_rtvIndex++;
	return rtvHandle;
}



JDx12Helper::~JDx12Helper()
{
}

J_RENDER_END