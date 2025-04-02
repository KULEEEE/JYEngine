#include "engine/JRenderTarget.h"
#include "engine/JEngineContext.h"

J_ENGINE_BEGIN

JRenderTarget::JRenderTarget()
	: _rtvCount(1) 
{
	_rtvHandles.resize(_rtvCount);
	_rtvResources.resize(_rtvCount);

	// create RTVResources

	_rtvHandles[0] = GetEngine()->GetDx12Helper()->CreateCPUDescriptorHandle(_rtvResources[0]);
}

//SwapChain인 경우만
JRenderTarget::JRenderTarget(ID3D12Resource* resource)
{
	_rtvResources.push_back(resource);

	const D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle = GetEngine()->GetDx12Helper()->CreateCPUDescriptorHandle(resource);
	_rtvHandles.push_back(rtvHandle);
	
	isSwapChainTarget = true;
}

JRenderTarget::~JRenderTarget()
{
	for (auto& rtv : _rtvResources)
	{
		delete rtv;
		rtv = nullptr;
	}

	_rtvResources.clear();
	_rtvHandles.clear();
}

vector<D3D12_CPU_DESCRIPTOR_HANDLE>& JRenderTarget::GetRTVHandle()
{
	return _rtvHandles;
}

vector<ID3D12Resource*>& JRenderTarget::GetRTVResource()
{
	return _rtvResources;
}

J_ENGINE_END