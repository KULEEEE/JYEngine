#pragma once
#ifndef __J_RENDER_RESOURCE_H__
#define __J_RENDER_RESOURCE_H__

#include "precompile.h"
#include "engine/JRenderDefinition.h"

J_ENGINE_BEGIN

struct JMeshResource
{
	vector<Render::JVertexBuffer*> soaBuffers;
	size_t vertexCount;
};
J_ENGINE_END

#endif