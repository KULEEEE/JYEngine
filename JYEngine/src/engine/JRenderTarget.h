#pragma once
#ifndef __J_RENDER_TARGET_H__
#define __J_RENDER_TARGET_H__

#include "engine/precompile.h"

J_ENGINE_BEGIN

class JRenderTarget
{
public:
	JRenderTarget();
	JRenderTarget(ID3D12Resource* resource);
	~JRenderTarget();

	vector<D3D12_CPU_DESCRIPTOR_HANDLE>& GetRTVHandle();
	vector<ID3D12Resource*>& GetRTVResource();

private:
	
	vector<D3D12_CPU_DESCRIPTOR_HANDLE> _rtvHandles;
	vector<ID3D12Resource*> _rtvResources;

	uint8 _rtvCount;

	bool isSwapChainTarget = false;
};

J_ENGINE_END

#endif