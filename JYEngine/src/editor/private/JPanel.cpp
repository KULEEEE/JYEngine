#include "editor/JPanel.h"
#include "engine/precompile.h"
#include "engine/JEngineContext.h"

J_EDITOR_BEGIN

void JPanel::Init()
{
}

void JPanel::Update()
{
	s_Engine->RenderBegin();
	s_Engine->RenderEnd();
}

J_EDITOR_END


