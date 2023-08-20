#pragma once

#ifndef __J_ENGINE_CONTEXT_H__
#define __J_ENGINE_CONTEXT_H__

#include "engine/JRenderDefinition.h"
#include "engine/JEngine.h"

extern J::Engine::JEngine* s_Engine;

static void InitializeEngine()
{
	s_Engine = new J::Engine::JEngine();
}

static void DestroyEngine()
{
	delete s_Engine;
}

#endif