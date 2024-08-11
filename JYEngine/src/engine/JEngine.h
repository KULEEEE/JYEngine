#pragma once
#ifndef __J_ENGINE_H__
#define __J_ENGINE_H__

#include "engine/JRenderDefinition.h"
#include "engine/JDevice.h"
#include "engine/JCommandQueue.h"
#include "engine/JSwapChain.h"
#include "engine/dx12/JDx12Helper.h"
#include "engine/JRenderContext.h"

J_ENGINE_BEGIN

class JEngine
{
public:

	JEngine() = default;
	JEngine(Render::JCommandQueue* cmdQueue, Render::JSwapChain* swapChain);
	~JEngine();

	Render::JDevice* GetDevice() { return _device; }
	Render::JCommandQueue* GetCmdQueue() { return _cmdQueue; }
	Render::JSwapChain* GetSwapChain() { return _swapChain; }
	Render::JDx12Helper* GetDx12Helper() { return _dx12Helper; }
	Render::JRenderContext* GetRenderContext() { return _renderContext; }

private:
	void initialize(Render::JCommandQueue* cmdQueue, Render::JSwapChain* swapChain);
	void destroy();

	Render::JDevice* _device;
	Render::JCommandQueue* _cmdQueue;
	Render::JSwapChain* _swapChain;
	Render::JDx12Helper* _dx12Helper;

	Render::JRenderContext* _renderContext;
};

J_ENGINE_END

#endif