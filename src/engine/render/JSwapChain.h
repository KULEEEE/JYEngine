#pragma once
#ifndef __J_SWAPCHAIN_H__
#define __J_SWAPCHAIN_H__

#include "engine/render/JRenderDefinition.h"
#include "engine/render/JRenderTarget.h"

/*#include "engine/platform/JDevice.h"*/ namespace J { namespace Render { class JDevice; } }

J_RENDER_BEGIN

class JSwapChain
{
public:
	JSwapChain();
	~JSwapChain();

	void Initialize(const JWindowInfo& info, const JDevice* device, ComPtr<ID3D12CommandQueue> cmdQueue);
	void Resize(uint32 width, uint32 height);
	void Present();
	void SwapIndex();

	ComPtr<IDXGISwapChain> GetSwapChain() { return _swapChain; }
	Engine::JRenderTarget* GetRenderTarget() { return _renderTargets[_backBufferIndex]; }
	uint32 GetBackBufferIndex() const { return _backBufferIndex; }

private:
	void createSwapChain(const JWindowInfo& info, ComPtr<IDXGIFactory> dxgi, ComPtr<ID3D12CommandQueue> cmdQueue);
	void destroyRenderTargets();

	ComPtr<IDXGISwapChain> _swapChain;
	Engine::JRenderTarget* _renderTargets[SWAP_CHAIN_BUFFER_COUNT] = {};
	uint32 _backBufferIndex = 0;
};

J_RENDER_END
#endif
