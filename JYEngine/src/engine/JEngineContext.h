#pragma once

#ifndef __J_ENGINE_CONTEXT_H__
#define __J_ENGINE_CONTEXT_H__

#include "engine/JRenderDefinition.h"
#include "engine/JEngine.h"

static J::Engine::JEngine* s_Engine = nullptr;

static void InitializeEngine(const J::Render::JWindowInfo& info)
{
	s_Engine = new J::Engine::JEngine(info);
}

static J::Engine::JEngine* GetEngine()
{
	return s_Engine;
}

static void DestroyEngine()
{
	delete s_Engine;
}

#endif