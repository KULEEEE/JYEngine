#pragma once

#ifndef __J_ENGINE_CONTEXT_H__
#define __J_ENGINE_CONTEXT_H__

#include "precompile.h"

/*#include "engine/JEngine.h"*/ namespace J{ namespace Engine { class JEngine; } }

extern J::Engine::JEngine* s_Engine;

static void InitializeEngine();

static void DestroyEngine();

#endif