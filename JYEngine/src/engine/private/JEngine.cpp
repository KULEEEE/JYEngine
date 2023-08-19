#include "engine/JEngine.h"
#include "engine/JDevice.h"
#include "engine/JSwapChain.h"
#include "engine/JDescriptorHeap.h"
#include "engine/JCommandQueue.h"

J_ENGINE_BEGINE

JEngine::JEngine(const JWindowInfo& window)
{
	initialize(window);
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

void JEngine::initialize(const JWindowInfo& window)
{
	_window = window;
	ResizeWindow(window.width, window.height);

	_viewport = { 0,0, static_cast<FLOAT>(window.width), static_cast<FLOAT>(window.height), 0.0f, 1.0f };
	_scissorRect = CD3DX12_RECT(0, 0, window.width, window.height);

	_device = new JDevice();

	
}

void JEngine::destroy()
{
	delete _device;
}
J_ENGINE_END