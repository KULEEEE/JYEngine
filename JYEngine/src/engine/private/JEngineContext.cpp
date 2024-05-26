#include "engine/JEngineContext.h"

J::Engine::JEngine* s_Engine = nullptr;

void InitializeEngine(J::Render::JCommandQueue* cmdQueue, J::Render::JSwapChain* swapChain, J::Render::JRootSignature* rootSignature)
{
	s_Engine = new J::Engine::JEngine(cmdQueue, swapChain, rootSignature);
}

J::Engine::JEngine* GetEngine()
{
	return s_Engine;
}

void DestroyEngine()
{
	delete s_Engine;
	s_Engine = nullptr;
}
