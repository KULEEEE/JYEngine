#pragma once
#ifndef __J_SCENE_HANDLE_H__
#define __J_SCENE_HANDLE_H__

#include "engine/precompile.h"

J_ENGINE_BEGIN

// 풀의 슬롯을 가리키는 경량 핸들.
// index는 슬롯 위치, generation은 삭제 후 재사용된 슬롯을 구분하는 값이다.
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

struct JRenderObjectComponentHandle
{
	uint32 index = static_cast<uint32>(-1);
	uint32 generation = 0;

	bool IsValid() const { return index != static_cast<uint32>(-1); }
};

struct JMaterialHandle
{
	uint32 index = static_cast<uint32>(-1);
	uint32 generation = 0;

	bool IsValid() const { return index != static_cast<uint32>(-1); }
};

J_ENGINE_END

#endif
