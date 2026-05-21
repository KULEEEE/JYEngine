#pragma once
#ifndef __J_DRAW_ITEM_CACHE_H__
#define __J_DRAW_ITEM_CACHE_H__

#include "engine/precompile.h"
#include "engine/render/JRenderFrame.h"

J_ENGINE_BEGIN

struct JDrawRange
{
	uint32 start = 0;
	uint32 count = 0;
	uint32 generation = 0;
	bool valid = false;
	bool tracked = false;
};

struct JDrawItemCache
{
	std::vector<JDrawItem> drawItems;
	std::vector<JDrawRange> drawRangeByEntityIndex;
	std::vector<uint32> activeDrawEntityIndices;
	bool initialized = false;
};

J_ENGINE_END

#endif
