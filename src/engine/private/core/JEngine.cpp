#include "engine/core/JEngine.h"
#include "engine/platform/JDevice.h"
#include "engine/render/JSwapChain.h"
#include "engine/render/JCommandQueue.h"

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
	_jobSystem = new JJobSystem();
	_jobSystem->Initialize();
	_renderServer = new JRenderServer();
	_renderServer->SetJobSystem(_jobSystem);
	_renderer = new JRenderer();
	_renderer->Initialize(_cmdQueue, _renderContext);
	_materialFactory = new JMaterialFactory(_renderContext);
}

void JEngine::destroy()
{
	delete _materialFactory;
	_materialFactory = nullptr;

	delete _renderer;
	_renderer = nullptr;

	delete _renderServer;
	_renderServer = nullptr;

	delete _jobSystem;
	_jobSystem = nullptr;

	delete _renderContext;
	_renderContext = nullptr;

	delete _dx12Helper;
	_dx12Helper = nullptr;

	delete _device;
	_device = nullptr;
}

J_ENGINE_END
