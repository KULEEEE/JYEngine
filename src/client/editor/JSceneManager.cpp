#include "client/editor/JSceneManager.h"

#include "third_party/nlohmann/json.hpp"

#include <fstream>
#include <iostream>
#include <utility>

J_EDITOR_BEGIN

JSceneManager::JSceneManager(Engine::JRenderer* renderer, Engine::JMaterialFactory* materialFactory, float cameraAspectRatio)
	: _assetManager(materialFactory)
	, _renderer(renderer)
{
	_buildContext.materialFactory = materialFactory;
	_buildContext.assetManager = &_assetManager;
	_buildContext.cameraAspectRatio = cameraAspectRatio;
}

JSceneManager::~JSceneManager()
{
	_sceneBuild.Release();
}

bool JSceneManager::buildFromSceneData(const Engine::JSceneData& sceneData)
{
	if (!JSceneBuilder::Build(sceneData, _buildContext, _sceneBuild))
	{
		return false;
	}

	return true;
}

Engine::JSceneData JSceneManager::createEmptySceneData(const std::string& sceneName) const
{
	Engine::JSceneData sceneData;
	sceneData.id = "untitled_scene";
	sceneData.name = sceneName;
	sceneData.version = 1;

	Engine::JSceneEntityData cameraEntity;
	cameraEntity.stableID = "ent_camera";
	cameraEntity.name = "Editor Camera";
	cameraEntity.active = true;
	cameraEntity.hasTransform = true;
	cameraEntity.transform.translation = { 0.0f, 0.0f, -8.0f };
	cameraEntity.transform.rotation = { 0.0f, 0.0f, 0.0f };
	cameraEntity.transform.scale = { 1.0f, 1.0f, 1.0f };
	cameraEntity.hasCamera = true;
	cameraEntity.camera.active = true;
	cameraEntity.camera.primary = true;
	cameraEntity.camera.nearP = 0.5f;
	cameraEntity.camera.farP = 1000.0f;
	sceneData.entities.push_back(cameraEntity);

	Engine::JSceneEntityData lightEntity;
	lightEntity.stableID = "ent_light";
	lightEntity.name = "Scene Light";
	lightEntity.active = true;
	lightEntity.hasTransform = true;
	lightEntity.transform.translation = { 0.0f, 4.0f, -4.0f };
	lightEntity.transform.rotation = { 0.0f, 0.0f, 0.0f };
	lightEntity.transform.scale = { 1.0f, 1.0f, 1.0f };
	lightEntity.hasLight = true;
	lightEntity.light.active = true;
	sceneData.entities.push_back(lightEntity);

	return sceneData;
}

bool JSceneManager::Open(const std::filesystem::path& filePath)
{
	set_Engine_Res_Path_Override({});
	_assetManager.SetResourceRoot(filePath.parent_path());
	Engine::JSceneData sceneData;
	if (!Engine::JSceneSerializer::LoadFromFile(filePath, sceneData))
	{
		std::cerr << "JSceneManager::Open failed: load failed: " << filePath.string() << std::endl;
		return false;
	}

	if (!buildFromSceneData(sceneData))
	{
		std::cerr << "JSceneManager::Open failed: build failed: " << filePath.string() << std::endl;
		return false;
	}

	_sceneData = std::move(sceneData);
	_currentPath = filePath;
	_dirty = false;
	return true;
}

bool JSceneManager::OpenProject(const std::filesystem::path& projectPath)
{
	const std::filesystem::path absoluteProjectPath = std::filesystem::absolute(projectPath);
	const std::filesystem::path projectFilePath = std::filesystem::is_directory(absoluteProjectPath)
		? absoluteProjectPath / "project.json"
		: absoluteProjectPath;

	std::ifstream stream(projectFilePath);
	if (!stream)
	{
		std::cerr << "JSceneManager::OpenProject failed: project file not found: " << projectFilePath.string() << std::endl;
		return false;
	}

	nlohmann::json root;
	stream >> root;

	const std::filesystem::path projectRoot = projectFilePath.parent_path();
	const std::filesystem::path startupScene = root.value("startupScene", "scenes/main.scene.json");
	const std::filesystem::path scenePath = startupScene.is_absolute() ? startupScene : projectRoot / startupScene;

	Engine::JSceneData sceneData;
	if (!Engine::JSceneSerializer::LoadFromFile(scenePath, sceneData))
	{
		std::cerr << "JSceneManager::OpenProject failed: scene load failed: " << scenePath.string() << std::endl;
		return false;
	}

	set_Engine_Res_Path_Override(projectRoot);
	_assetManager.SetResourceRoot(projectRoot);
	if (!buildFromSceneData(sceneData))
	{
		std::cerr << "JSceneManager::OpenProject failed: build failed: " << scenePath.string() << std::endl;
		return false;
	}

	_sceneData = std::move(sceneData);
	_currentPath = scenePath;
	_dirty = false;
	return true;
}

bool JSceneManager::Save()
{
	if (_currentPath.empty())
	{
		return false;
	}

	if (!Engine::JSceneSerializer::SaveToFile(_currentPath, _sceneData))
	{
		std::cerr << "JSceneManager::Save failed: save failed: " << _currentPath.string() << std::endl;
		return false;
	}

	_dirty = false;
	return true;
}

bool JSceneManager::SaveAs(const std::filesystem::path& filePath)
{
	if (!Engine::JSceneSerializer::SaveToFile(filePath, _sceneData))
	{
		std::cerr << "JSceneManager::SaveAs failed: save failed: " << filePath.string() << std::endl;
		return false;
	}

	_currentPath = filePath;
	_dirty = false;
	return true;
}

bool JSceneManager::New(const std::string& sceneName)
{
	const Engine::JSceneData sceneData = createEmptySceneData(sceneName);
	if (!buildFromSceneData(sceneData))
	{
		std::cerr << "JSceneManager::New failed: build failed." << std::endl;
		return false;
	}

	_sceneData = sceneData;
	_currentPath.clear();
	_dirty = true;
	return true;
}

bool JSceneManager::Reload()
{
	if (_currentPath.empty())
	{
		return false;
	}

	return Open(_currentPath);
}

Engine::JScene* JSceneManager::GetScene()
{
	return _sceneBuild.GetScene();
}

const Engine::JScene* JSceneManager::GetScene() const
{
	return _sceneBuild.GetScene();
}

Engine::JCameraHandle JSceneManager::GetPrimaryCamera() const
{
	return _sceneBuild.primaryCamera;
}

Engine::JLightHandle JSceneManager::GetFirstLight() const
{
	return _sceneBuild.firstLight;
}

J_EDITOR_END
