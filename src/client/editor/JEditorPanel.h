#pragma once
#ifndef __J_EDITOR_PANEL_H__
#define __J_EDITOR_PANEL_H__

#include "engine/precompile.h"

J_EDITOR_BEGIN

class JEditorPanel
{
public:
	virtual ~JEditorPanel() = default;
	virtual void Init() = 0;
	virtual void Update() = 0;
};

J_EDITOR_END

#endif
