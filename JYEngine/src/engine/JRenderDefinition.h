#pragma once
#ifndef __J_RENDER_DEFINITION_H__
#define __J_RENDER_DEFINITION_H__

#include "engine/precompile.h"

#define SWAP_CHAIN_BUFFER_COUNT 2
#define MAX_DESCRIPTOR_COUNT 1215

J_RENDER_BEGIN
struct JWindowInfo
{
	HWND hwnd;  // output Window
	int32 width;
	int32 height;
	bool windowed; // true: Windowed Mode, false: Fullscreen Mode
};

using JViewport = D3D12_VIEWPORT;

struct JVertexBuffer
{
	ID3D12Resource* buffer;
	D3D12_VERTEX_BUFFER_VIEW* view;

	void Destroy()
	{

	}
};
J_RENDER_END
#endif