#include "engine/core/JEngineContext.h"

J::Engine::JEngine* s_Engine = nullptr;
J::Render::JWindowInfo s_EngineWindowInfo = {};

void InitializeEngine(J::Render::JCommandQueue* cmdQueue, J::Render::JSwapChain* swapChain)
{
	s_Engine = new J::Engine::JEngine(cmdQueue, swapChain);
}

J::Engine::JEngine* GetEngine()
{
	return s_Engine;
}

J::Render::JWindowInfo& GetEngineWindowInfo()
{
	return s_EngineWindowInfo;
}

HWND GetEngineWindowHandle()
{
	return s_EngineWindowInfo.hwnd;
}

void DestroyEngine()
{
	delete s_Engine;
	s_Engine = nullptr;
	s_EngineWindowInfo = {};
}
