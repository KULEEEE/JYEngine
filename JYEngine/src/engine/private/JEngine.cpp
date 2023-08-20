#include "engine/JEngine.h"
#include "engine/JDevice.h"
#include "engine/JSwapChain.h"
#include "engine/JDescriptorHeap.h"
#include "engine/JCommandQueue.h"

J_ENGINE_BEGIN

using namespace J::Render;

JEngine::JEngine()
{
	initialize();
}

JEngine::~JEngine()
{
	destroy();
}

void JEngine::ResizeWindow(int32 width, int32 height)
{
	_window.width = width;
	_window.height = height;

	RECT rect = { 0,0, width, height };
	::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
	::SetWindowPos(_window.hwnd, 0, 100, 100, width, height, 0);
}

void JEngine::RenderBegin()
{
	_cmdQueue->RenderBegin(&_viewport, &_scissorRect);
}

void JEngine::RenderEnd()
{
	_cmdQueue->RenderEnd();
}

void JEngine::initialize()
{
	int32 width = 800;
	int32 height = 600;
	_window.width = width;
	_window.height = height;
	_window.windowed = true;

	ResizeWindow(_window.width, _window.height);

	_viewport = { 0,0, static_cast<FLOAT>(_window.width), static_cast<FLOAT>(_window.height), 0.0f, 1.0f };
	_scissorRect = CD3DX12_RECT(0, 0, _window.width, _window.height);

	_device = new JDevice();
	_cmdQueue = new JCommandQueue();
	_descriptorHeap = new JDescriptorHeap();
	_swapChain = new JSwapChain();

	_cmdQueue->Initialize(_device->GetDevice(), _swapChain, _descriptorHeap);
	_swapChain->Initialize(_window, _device->GetDXGI(), _cmdQueue->GetCmdQueue());
	_descriptorHeap->Initialize(_device->GetDevice(), _swapChain);
	
}

void JEngine::destroy()
{
	delete _device;
	delete _cmdQueue;
	delete _descriptorHeap;
	delete _swapChain;
}
J_ENGINE_END