#pragma once
#ifndef __J_SCENE_MANAGER_H__
#define __J_SCENE_MANAGER_H__

#include "client/editor/JSceneBuilder.h"
#include "engine/scene/JSceneSerializer.h"
#include "engine/render/JRenderer.h"

J_EDITOR_BEGIN

class JSceneManager
{
public:
	JSceneManager(Engine::JRenderer* renderer, Engine::JMaterialFactory* materialFactory, float cameraAspectRatio = 1.0f);
	~JSceneManager();

	JSceneManager(const JSceneManager&) = delete;
	JSceneManager& operator=(const JSceneManager&) = delete;

	bool Open(const std::filesystem::path& filePath);
	bool Save();
	bool SaveAs(const std::filesystem::path& filePath);
	bool New(const std::string& sceneName = "Untitled Scene");
	bool Reload();

	Engine::JScene* GetScene();
	const Engine::JScene* GetScene() const;
	Engine::JCameraHandle GetPrimaryCamera() const;
	Engine::JLightHandle GetFirstLight() const;
	const std::filesystem::path& GetCurrentPath() const { return _currentPath; }
	bool HasScene() const { return GetScene() != nullptr; }
	bool IsDirty() const { return _dirty; }
	void MarkDirty() { _dirty = true; }

private:
	bool buildFromSceneData(const Engine::JSceneData& sceneData);
	Engine::JSceneData createEmptySceneData(const std::string& sceneName) const;

	JAssetManager _assetManager;
	JSceneBuildContext _buildContext;
	JSceneBuildResult _sceneBuild;
	Engine::JRenderer* _renderer = nullptr;
	Engine::JSceneData _sceneData;
	std::filesystem::path _currentPath;
	bool _dirty = false;
};

J_EDITOR_END

#endif
