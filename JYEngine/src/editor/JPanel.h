#pragma once
#ifndef __JPANEL_H__
#define __JPANEL_H__

#include "engine/precompile.h"

/*#include "engine/JEngine.h"*/ namespace J { namespace Engine { class JEngine; } }

J_EDITOR_BEGIN

class JPanel
{
public:
	void Init();
	void Update(Engine::JEngine* engine);

private:
	
};

J_EDITOR_END
#endif