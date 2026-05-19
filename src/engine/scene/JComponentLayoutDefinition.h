#pragma once
#ifndef __J_COMPONENT_LAYOUT_DEFINITION_H__
#define __J_COMPONENT_LAYOUT_DEFINITION_H__

#include "engine/precompile.h"
#include "engine/scene/JSceneHandle.h"
/*#include "engine/asset/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }

J_ENGINE_BEGIN

struct JTransformComponents
{
	JVec3 translation = { 0.0f, 0.0f, 0.0f };
	JVec3 rotation = { 0.0f, 0.0f, 0.0f };
	JVec3 scale = { 1.0f, 1.0f, 1.0f };
};

struct JCameraComponents
{
	JEntityHandle entity = {};
	float aspectRatio = 1.0f;
	float nearP = 0.5f;
	float farP = 1000.0f;
	bool active = true;
};

struct JLightComponents
{
	JEntityHandle entity = {};
	JVec3 color = { 1.0f, 1.0f, 1.0f };
	float intensity = 1.0f;
	bool active = true;
};

struct JDrawComponent
{
	JEntityHandle entity = {};
	const JMesh* mesh = nullptr;
	uint32 subMeshIndex = 0;
	uint32 materialID = 0;
	bool visible = true;
	bool transparent = false;
	bool active = true;
};

J_ENGINE_END

#endif
