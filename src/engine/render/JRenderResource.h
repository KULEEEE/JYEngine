#pragma once
#ifndef __J_RENDER_RESOURCE_H__
#define __J_RENDER_RESOURCE_H__

#include "engine/precompile.h"
#include "engine/render/JRenderDefinition.h"

J_ENGINE_BEGIN

struct JMeshResource
{
	vector<Render::JVertexBuffer*> vertexBuffers;
	Render::JIndexBuffer* indexBufferResource = nullptr;
	vector<D3D12_VERTEX_BUFFER_VIEW> soaBuffers;
	D3D12_INDEX_BUFFER_VIEW indexBuffer;
	size_t indexSize = 0;
	bool hasNormals = false;
	bool hasTexcoords = false;
};
J_ENGINE_END

#endif
