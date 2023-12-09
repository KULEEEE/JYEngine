#pragma once
#ifndef __J_ENGINE_H__
#define __J_ENGINE_H__

#include "engine/JRenderDefinition.h"

/*#include "engine/JDevice.h"*/ namespace J { namespace Render { class JDevice; } }
/*#include "engine/JDevice.h"*/ namespace J { namespace Render { class JCommandQueue; } }
/*#include "engine/JDevice.h"*/ namespace J { namespace Render { class JSwapChain; } }
/*#include "engine/JDevice.h"*/ namespace J { namespace Render { class JDescriptorHeap; } }

J_ENGINE_BEGIN

class JEngine
{
public:

	JEngine(const Render::JWindowInfo& info);
	~JEngine();
	void ResizeWindow(int32 width, int32 height);

	//TODO: Delete
	void RenderBegin();
	void RenderEnd();

	//TODO: 나중에는 이걸로 Command Queue를 직접 작성하는게 맞다.

private:
	void initialize(const Render::JWindowInfo& info);
	void destroy();

	Render::JWindowInfo		_window;
	D3D12_VIEWPORT	_viewport = {};
	D3D12_RECT		_scissorRect = {};

	Render::JDevice* _device;
	Render::JCommandQueue* _cmdQueue;
	Render::JSwapChain* _swapChain;
	Render::JDescriptorHeap* _descriptorHeap;

};

J_ENGINE_END

#endif