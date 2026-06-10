#pragma once

#ifndef __J_ENGINE_CONTEXT_H__
#define __J_ENGINE_CONTEXT_H__

#include "engine/render/JRenderDefinition.h"
#include "engine/core/JEngine.h"

extern J::Engine::JEngine * s_Engine;

void InitializeEngine(J::Render::JCommandQueue* cmdQueue, J::Render::JSwapChain* swapChain);

J::Engine::JEngine* GetEngine();
J::Render::JWindowInfo& GetEngineWindowInfo();
HWND GetEngineWindowHandle();

void DestroyEngine();

#endif
