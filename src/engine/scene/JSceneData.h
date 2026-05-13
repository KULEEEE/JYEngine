#pragma once
#ifndef __J_SCENE_DATA_H__
#define __J_SCENE_DATA_H__

#include "engine/precompile.h"

J_ENGINE_BEGIN

enum class JSceneMeshType : uint8
{
	External,
	Plane,
};

struct JSceneMaterialTextureData
{
	std::string name;
	std::string path;
};

struct JSceneMaterialConstantsData
{
	bool enabled = false;
	JVec4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float roughness = 0.5f;
	float metallic = 1.0f;
};

struct JSceneMaterialData
{
	std::string id;
	std::string name;
	std::string shaderPath;
	bool enableAlphaBlend = false;
	JSceneMaterialConstantsData constants;
	std::vector<JSceneMaterialTextureData> textures;
};

struct JSceneMeshPlaneData
{
	float size = 1.0f;
	float y = 0.0f;
};

struct JSceneMeshData
{
	std::string id;
	std::string name;
	JSceneMeshType type = JSceneMeshType::External;
	std::string path;
	JSceneMeshPlaneData plane;
};

struct JSceneTransformData
{
	JVec3 position = { 0.0f, 0.0f, 0.0f };
	float yaw = 0.0f;
	float pitch = 0.0f;
};

struct JSceneCameraData
{
	bool active = true;
	bool primary = false;
	float moveSpeed = 0.1f;
	float rotateSpeed = 1.0f;
};

struct JSceneLightData
{
	bool active = true;
	JVec3 color = { 1.0f, 1.0f, 1.0f };
	float intensity = 1.0f;
};

struct JSceneRenderObjectData
{
	bool active = true;
	bool visible = true;
	bool transparent = false;
	std::string materialID;
	std::string meshID;
};

struct JSceneEntityData
{
	std::string stableID;
	std::string name;
	bool active = true;
	std::vector<std::string> tags;

	bool hasTransform = false;
	JSceneTransformData transform;

	bool hasCamera = false;
	JSceneCameraData camera;

	bool hasLight = false;
	JSceneLightData light;

	bool hasRenderObject = false;
	JSceneRenderObjectData renderObject;
};

struct JSceneData
{
	std::string id;
	std::string name;
	uint32 version = 1;
	std::vector<JSceneMaterialData> materials;
	std::vector<JSceneMeshData> meshes;
	std::vector<JSceneEntityData> entities;
};

J_ENGINE_END

#endif
