#include "engine/JEngine.h"
#include "engine/JDevice.h"
#include "engine/JSwapChain.h"
#include "engine/JCommandQueue.h"

J_ENGINE_BEGIN

using namespace J::Render;

JEngine::JEngine(JCommandQueue* cmdQueue, JSwapChain* swapChain)
{
	initialize(cmdQueue, swapChain);
}

JEngine::~JEngine()
{
	destroy();
}

void JEngine::initialize(JCommandQueue* cmdQueue, JSwapChain* swapChain)
{
	_device = new JDevice();
	_dx12Helper = new JDx12Helper(_device->GetDevice());
	_cmdQueue = cmdQueue;
	_swapChain = swapChain;
	_renderContext = new JRenderContext(_device);
	_renderServer = new JRenderServer();
}

void JEngine::destroy()
{
	delete _renderServer;
	_renderServer = nullptr;

	delete _renderContext;
	_renderContext = nullptr;

	delete _dx12Helper;
	_dx12Helper = nullptr;

	delete _device;
	_device = nullptr;
}

J_ENGINE_END
