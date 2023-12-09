#include "editor/JPanel.h"
#include "engine/precompile.h"
#include "engine/JEngineContext.h"

J_EDITOR_BEGIN

void JPanel::Init()
{
}

void JPanel::Update(Engine::JEngine* engine)
{
	engine->RenderBegin();
	engine->RenderEnd();
}

J_EDITOR_END


