#pragma once
#ifndef __J_RENDER_RESOURCE_H__
#define __J_RENDER_RESOURCE_H__

#include "engine/precompile.h"
#include "engine/render/JRenderDefinition.h"
#include "engine/scene/JSceneHandle.h"

J_ENGINE_BEGIN

struct JCameraResource
{
	JCameraHandle camera = {};
	XMFLOAT4X4 viewProjection = {};
	Render::JConstantBuffer* perFrameBuffer = nullptr;
};

struct JTransformResource
{
	JTransformHandle transform = {};
	XMFLOAT4X4 world = {};
	Render::JConstantBuffer* perObjectBuffer = nullptr;
};

struct JLightResource
{
	Render::JConstantBuffer* lightBuffer = nullptr;
	JVec4 colorIntensity = { 1.0f, 1.0f, 1.0f, 0.35f };
	JVec4 positionCount = { 0.0f, 4.0f, -4.0f, 0.0f };
	uint32 lightCount = 0;
};

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
