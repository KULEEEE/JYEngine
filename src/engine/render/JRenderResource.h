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

struct JPerObjectConstants
{
	XMFLOAT4X4 world;
};

struct JPerFrameConstants
{
	XMFLOAT4X4 viewProjection;
};

struct JPerViewConstants
{
	XMFLOAT4X4 inverseViewProjection;
	JVec4 cameraPosition;
};

struct JLightConstants
{
	static constexpr uint32 MAX_LIGHTS = 8;

	JVec4 colorIntensities[MAX_LIGHTS];
	JVec4 positions[MAX_LIGHTS];
	JVec4 info;
};

struct JShadowConstants
{
	XMFLOAT4X4 cascadeViewProjections[4];
	JVec4 cascadeSplits;  // 카메라 거리 기준 cascade 경계
	JVec4 cascadeBiases;  // reverse-Z depth 단위 bias (cascade별)
	JVec4 shadowParams;   // x: enabled, y: max shadow distance
};

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
		std::string path;
	};

	JMaterialHandle material = {};
	Render::JShader* shader = nullptr;
	std::string shaderPath;
	Render::JPipeline* pipeline = nullptr;
	std::vector<ConstantBufferEntry> constantBuffers;
	std::vector<TextureEntry> textures;
};

struct JCameraResource
{
	JCameraHandle camera = {};
	JPerFrameConstants constants = {};
};

struct JTransformResource
{
	JTransformHandle transform = {};
	XMMATRIX world = XMMatrixIdentity();
	JPerObjectConstants constants = {};
};

struct JLightResource
{
	JLightConstants constants = {};
	uint32 lightCount = 0;
	bool initialized = false;
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
