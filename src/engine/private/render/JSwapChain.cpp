#include "engine/render/JSwapChain.h"
#include "engine/platform/JDevice.h"

#include <algorithm>
#include <iostream>

J_RENDER_BEGIN

JSwapChain::JSwapChain() = default;

JSwapChain::~JSwapChain()
{
	destroyRenderTargets();
}

void JSwapChain::destroyRenderTargets()
{
	for (uint32 i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
	{
		delete _renderTargets[i];
		_renderTargets[i] = nullptr;
	}
}

void JSwapChain::Present()
{
	if (_swapChain == nullptr)
	{
		std::cerr << "Present skipped: swap chain is null." << std::endl;
		return;
	}

	_swapChain->Present(0, 0);
}

void JSwapChain::SwapIndex()
{
	_backBufferIndex = (_backBufferIndex + 1) % SWAP_CHAIN_BUFFER_COUNT;
}

void JSwapChain::Initialize(const JWindowInfo& info, const JDevice* device, ComPtr<ID3D12CommandQueue> cmdQueue)
{
	createSwapChain(info, device->GetDXGI(), cmdQueue);
}

void JSwapChain::Resize(uint32 width, uint32 height)
{
	if (_swapChain == nullptr)
	{
		return;
	}

	width = std::max(width, 1u);
	height = std::max(height, 1u);

	ComPtr<IDXGISwapChain3> swapChain3;
	_swapChain.As(&swapChain3);
	if (swapChain3 != nullptr)
	{
		_backBufferIndex = swapChain3->GetCurrentBackBufferIndex();
	}

	destroyRenderTargets();

	const HRESULT resizeHr = _swapChain->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
	if (FAILED(resizeHr))
	{
		std::cerr << "JSwapChain::Resize failed. HRESULT=0x" << std::hex << resizeHr << std::dec << std::endl;
		return;
	}

	for (uint32 i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
	{
		ID3D12Resource* rtvResource = nullptr;
		const HRESULT bufferHr = _swapChain->GetBuffer(i, IID_PPV_ARGS(&rtvResource));
		if (FAILED(bufferHr) || rtvResource == nullptr)
		{
			std::cerr << "JSwapChain::Resize GetBuffer failed for buffer " << i << ". HRESULT=0x" << std::hex << bufferHr << std::dec << std::endl;
			continue;
		}

		_renderTargets[i] = new Engine::JRenderTarget(rtvResource);
	}

	_backBufferIndex = 0;
}

void JSwapChain::createSwapChain(const JWindowInfo& info, ComPtr<IDXGIFactory> dxgi, ComPtr<ID3D12CommandQueue> cmdQueue)
{
	if (dxgi == nullptr || cmdQueue == nullptr)
	{
		std::cerr << "createSwapChain failed: dxgi or command queue is null." << std::endl;
		return;
	}

	destroyRenderTargets();
	_swapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferDesc.Width = static_cast<UINT>(info.width);
	sd.BufferDesc.Height = static_cast<UINT>(info.height);
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
	sd.OutputWindow = info.hwnd;
	sd.Windowed = info.windowed;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	const HRESULT swapChainHr = dxgi->CreateSwapChain(cmdQueue.Get(), &sd, &_swapChain);
	if (FAILED(swapChainHr) || _swapChain == nullptr)
	{
		std::cerr << "CreateSwapChain failed. HRESULT=0x" << std::hex << swapChainHr << std::dec << std::endl;
		return;
	}

	for (uint32 i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
	{
		ID3D12Resource* rtvResource = nullptr;
		const HRESULT bufferHr = _swapChain->GetBuffer(i, IID_PPV_ARGS(&rtvResource));
		if (FAILED(bufferHr) || rtvResource == nullptr)
		{
			std::cerr << "GetBuffer failed for swap chain buffer " << i << ". HRESULT=0x" << std::hex << bufferHr << std::dec << std::endl;
			continue;
		}

		_renderTargets[i] = new Engine::JRenderTarget(rtvResource);
	}

	_backBufferIndex = 0;
}

J_RENDER_END
