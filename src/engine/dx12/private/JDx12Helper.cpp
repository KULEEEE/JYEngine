#include "engine/dx12/jdx12Helper.h"
#include "engine/render/JRenderDefinition.h"

J_RENDER_BEGIN

JDx12Helper::JDx12Helper(ComPtr<ID3D12Device> device)
{
	_device = device;

	D3D12_DESCRIPTOR_HEAP_DESC rtvDesc;
	rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDesc.NumDescriptors = MAX_DESCRIPTOR_COUNT;
	rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDesc.NodeMask = 0;

	//     
	// RTV  : [ ] [ ]
	device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&_rtvHeap));

	_rtvHeapSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_rtvHeapBegin = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE JDx12Helper::CreateCPUDescriptorHandle(ID3D12Resource* rtvResource)
{
	return CreateCPUDescriptorHandle(rtvResource, nullptr);
}

D3D12_CPU_DESCRIPTOR_HANDLE JDx12Helper::CreateCPUDescriptorHandle(ID3D12Resource* rtvResource, const D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc)
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
	rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(_rtvHeapBegin,  _rtvIndex*_rtvHeapSize);
	_device->CreateRenderTargetView(rtvResource, rtvDesc, rtvHandle);
	_rtvIndex++;
	return rtvHandle;
}



JDx12Helper::~JDx12Helper()
{
}

J_RENDER_END
