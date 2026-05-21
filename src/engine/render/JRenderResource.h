#pragma once
#ifndef __J_RENDER_RESOURCE_H__
#define __J_RENDER_RESOURCE_H__

#include "engine/precompile.h"
#include "engine/render/JRenderDefinition.h"
#include "engine/scene/JSceneHandle.h"

namespace J::Render
{
	class JShader;
}

J_ENGINE_BEGIN

struct JMaterialResource
{
	struct ConstantBufferEntry
	{
		std::string name;
		uint32 nameHash = 0;
		Render::JConstantBuffer* buffer = nullptr;
	};

	struct TextureEntry
	{
		std::string name;
		uint32 nameHash = 0;
		Render::JTexture* texture = nullptr;
	};

	uint32 materialID = 0;
	Render::JShader* shader = nullptr;
	Render::JPipeline* pipeline = nullptr;
	std::vector<ConstantBufferEntry> constantBuffers;
	std::vector<TextureEntry> textures;
};

struct JCameraResource
{
	JCameraHandle camera = {};
	Render::JConstantBuffer* perFrameBuffer = nullptr;
};

struct JTransformResource
{
	JTransformHandle transform = {};
	XMMATRIX world = XMMatrixIdentity();
	Render::JConstantBuffer* perObjectBuffer = nullptr;
};

struct JLightResource
{
	Render::JConstantBuffer* lightBuffer = nullptr;
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
