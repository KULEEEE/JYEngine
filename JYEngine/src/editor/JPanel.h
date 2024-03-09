#pragma once
#ifndef __JPANEL_H__
#define __JPANEL_H__

#include "Engine/precompile.h"
#include "Engine/JEngineContext.h"
#include "Engine/JCommandQueue.h"

J_EDITOR_BEGIN

class JPanel
{
public:

	JPanel() = default;
	~JPanel();
	void Init();
	void Update();

private:
	Render::JCommandQueue* _commandQueue;

};

J_EDITOR_END
#endif