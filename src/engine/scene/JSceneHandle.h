#pragma once
#ifndef __J_SCENE_HANDLE_H__
#define __J_SCENE_HANDLE_H__

#include "engine/precompile.h"

J_ENGINE_BEGIN

struct JEntityHandle
{
	uint32 index = static_cast<uint32>(-1);
	uint32 generation = 0;

	bool IsValid() const { return index != static_cast<uint32>(-1); }
};

struct JTransformHandle
{
	uint32 index = static_cast<uint32>(-1);
	uint32 generation = 0;

	bool IsValid() const { return index != static_cast<uint32>(-1); }
};

struct JCameraHandle
{
	uint32 index = static_cast<uint32>(-1);
	uint32 generation = 0;

	bool IsValid() const { return index != static_cast<uint32>(-1); }
};

struct JLightHandle
{
	uint32 index = static_cast<uint32>(-1);
	uint32 generation = 0;

	bool IsValid() const { return index != static_cast<uint32>(-1); }
};

struct JDrawComponentHandle
{
	uint32 index = static_cast<uint32>(-1);
	uint32 generation = 0;

	bool IsValid() const { return index != static_cast<uint32>(-1); }
};

J_ENGINE_END

#endif
