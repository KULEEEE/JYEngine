#pragma once

#ifndef __J_ENGINE_CONTEXT_H__
#define __J_ENGINE_CONTEXT_H__

#include "engine/JRenderDefinition.h"
#include "engine/JEngine.h"

extern J::Engine::JEngine * s_Engine;

void InitializeEngine(J::Render::JCommandQueue* cmdQueue, J::Render::JSwapChain* swapChain);

J::Engine::JEngine* GetEngine();

void DestroyEngine();

#endif