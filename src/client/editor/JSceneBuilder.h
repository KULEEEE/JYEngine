#pragma once
#ifndef __J_SCENE_BUILDER_H__
#define __J_SCENE_BUILDER_H__

#include "engine/scene/JScene.h"
#include "engine/scene/JSceneData.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"
#include "engine/render/JRenderDefinition.h"

#include "client/editor/JAssetManager.h"
/*#include "engine/render/JMaterialFactory.h"*/ namespace J { namespace Engine { class JMaterialFactory; } }
/*#include "engine/render/JRenderServer.h"*/ namespace J { namespace Engine { class JRenderServer; } }

J_EDITOR_BEGIN

struct JSceneBuildContext
{
	JAssetManager* assetManager = nullptr;
	Engine::JMaterialFactory* materialFactory = nullptr;
	Engine::JRenderServer* renderServer = nullptr;
	float cameraAspectRatio = 1.0f;
};

class JSceneBuildResult
{
public:
	JSceneBuildResult() = default;
	JSceneBuildResult(const JSceneBuildResult&) = delete;
	JSceneBuildResult& operator=(const JSceneBuildResult&) = delete;
	JSceneBuildResult(JSceneBuildResult&&) noexcept = default;
	JSceneBuildResult& operator=(JSceneBuildResult&&) noexcept = default;

	Engine::JScene* GetScene() const { return scene.get(); }
	void Release(Engine::JRenderServer* renderServer);

	std::unique_ptr<Engine::JScene> scene;
	std::vector<std::shared_ptr<Engine::JMaterial>> materials;
	std::vector<std::shared_ptr<Engine::JMesh>> meshes;
	std::vector<std::shared_ptr<Render::JConstantBuffer>> constantBuffers;
	std::vector<std::shared_ptr<Render::JTexture>> textures;
	std::vector<std::shared_ptr<JAssetManager::MaterialBundle>> materialBundles;

	std::unordered_map<std::string, uint32> materialIDs;
	std::unordered_map<std::string, Engine::JEntityHandle> entities;
	std::unordered_map<std::string, Engine::JTransformHandle> transforms;
	std::unordered_map<std::string, Engine::JCameraHandle> cameras;
	std::unordered_map<std::string, Engine::JLightHandle> lights;
	std::unordered_map<std::string, Engine::JRenderObjectHandle> renderObjects;
	std::vector<Engine::JCameraHandle> registeredCameras;

	Engine::JCameraHandle primaryCamera = {};
	Engine::JLightHandle firstLight = {};
};

class JSceneBuilder
{
public:
	static bool Build(const Engine::JSceneData& sceneData, const JSceneBuildContext& context, JSceneBuildResult& outResult);
};

J_EDITOR_END

#endif
