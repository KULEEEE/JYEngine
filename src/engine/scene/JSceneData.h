#pragma once
#ifndef __J_SCENE_DATA_H__
#define __J_SCENE_DATA_H__

#include "engine/precompile.h"

J_ENGINE_BEGIN

// 파일에 저장되는 Scene 데이터 구조.
// 런타임 풀 구조와 분리해서 JSON 로드/저장 포맷을 단순하게 유지한다.
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
	std::string path;
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
	JVec3 translation = { 0.0f, 0.0f, 0.0f };
	JVec3 rotation = { 0.0f, 0.0f, 0.0f };
	JVec3 scale = { 1.0f, 1.0f, 1.0f };
};

struct JSceneCameraData
{
	bool active = true;
	bool primary = false;
	float nearP = 5.0f;
	float farP = 1000.0f;
};

enum class JSceneLightType : uint8
{
	Point,
	Directional,
};

struct JSceneLightData
{
	bool active = true;
	JSceneLightType type = JSceneLightType::Point;
	JVec3 color = { 1.0f, 1.0f, 1.0f };
	float intensity = 1.0f;
	float range = 25.0f; // point 전용. directional은 transform rotation의 +Z 방향으로 비춘다.
};

struct JSceneRenderObjectComponentData
{
	bool active = true;
	bool visible = true;
	bool transparent = false;
	std::string materialID;
	std::vector<std::string> subMeshMaterialIDs;
	std::string meshID;
};

struct JSceneEntityData
{
	std::string stableID;
	std::string name;
	bool active = true;
	std::vector<std::string> tags;

	// has* 플래그가 true인 컴포넌트만 실제 Scene에 생성한다.
	bool hasTransform = false;
	JSceneTransformData transform;

	bool hasCamera = false;
	JSceneCameraData camera;

	bool hasLight = false;
	JSceneLightData light;

	bool hasRenderObjectComponent = false;
	JSceneRenderObjectComponentData renderObjectComponent;
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
