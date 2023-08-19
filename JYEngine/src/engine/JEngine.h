#pragma once
#ifndef __J_ENGINE_H__
#define __J_ENGINE_H__

#include "engine/precompile.h"

/*#include "engine/JDevice.h"*/ namespace J { namespace Engine { class JDevice; } }
/*#include "engine/JDevice.h"*/ namespace J { namespace Engine { class JCommandQueue; } }
/*#include "engine/JDevice.h"*/ namespace J { namespace Engine { class JSwapChain; } }
/*#include "engine/JDevice.h"*/ namespace J { namespace Engine { class JDescriptorHeap; } }

J_ENGINE_BEGINE

class JEngine
{
public:

	struct JWindowInfo
	{
		HWND hwnd;  // output Window
		int32 width;
		int32 height;
		bool windowed; // true: Windowed Mode, false: Fullscreen Mode
	};

	JEngine(const JWindowInfo& window);
	~JEngine();
	void ResizeWindow(int32 width, int32 height);

private:
	void initialize(const JWindowInfo& window);
	void destroy();

	JWindowInfo		_window;
	D3D12_VIEWPORT	_viewport = {};
	D3D12_RECT		_scissorRect = {};

	JDevice* _device;
	JCommandQueue* _cmdQueue;
	JSwapChain* _swapChain;
	JDescriptorHeap* _descriptorHeap;

};

J_ENGINE_END

#endif