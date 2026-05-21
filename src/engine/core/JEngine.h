#pragma once
#ifndef __J_ENGINE_H__
#define __J_ENGINE_H__

#include "engine/render/JRenderDefinition.h"
#include "engine/platform/JDevice.h"
#include "engine/render/JCommandQueue.h"
#include "engine/render/JSwapChain.h"
#include "engine/dx12/JDx12Helper.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JRenderServer.h"
#include "engine/render/JRenderer.h"
#include "engine/render/JMaterialFactory.h"
#include "engine/core/JJobSystem.h"

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
	JRenderServer* GetRenderServer() { return _renderServer; }
	JRenderer* GetRenderer() { return _renderer; }
	JMaterialFactory* GetMaterialFactory() { return _materialFactory; }
	JJobSystem* GetJobSystem() { return _jobSystem; }

private:
	void initialize(Render::JCommandQueue* cmdQueue, Render::JSwapChain* swapChain);
	void destroy();

	Render::JDevice* _device = nullptr;
	Render::JCommandQueue* _cmdQueue = nullptr;
	Render::JSwapChain* _swapChain = nullptr;
	Render::JDx12Helper* _dx12Helper = nullptr;
	Render::JRenderContext* _renderContext = nullptr;
	JJobSystem* _jobSystem = nullptr;
	JRenderServer* _renderServer = nullptr;
	JRenderer* _renderer = nullptr;
	JMaterialFactory* _materialFactory = nullptr;
};

J_ENGINE_END

#endif
