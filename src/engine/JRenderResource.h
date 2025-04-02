#pragma once
#ifndef __J_RENDER_RESOURCE_H__
#define __J_RENDER_RESOURCE_H__

#include "precompile.h"
#include "engine/JRenderDefinition.h"

J_ENGINE_BEGIN

struct JMeshResource
{
	vector<D3D12_VERTEX_BUFFER_VIEW> soaBuffers;
	D3D12_INDEX_BUFFER_VIEW indexBuffer;
	size_t indexSize;
};
J_ENGINE_END

#endif