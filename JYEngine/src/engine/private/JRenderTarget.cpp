#include "engine/JRenderTarget.h"
#include "engine/JEngineContext.h"

J_ENGINE_BEGIN

JRenderTarget::JRenderTarget()
	: _rtvCount(1) 
{

	_rtvHandles.resize(_rtvCount);
	_rtvResources.resize(_rtvCount);

	GetEngine()->GetDx12Helper()->CreateRenderTargetView(_rtvResources[0], _rtvHandles[0]);
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