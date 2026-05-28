#pragma once
#ifndef __J_ASSET_MANAGER_H__
#define __J_ASSET_MANAGER_H__

#include "engine/precompile.h"
#include "engine/scene/JSceneData.h"
#include "engine/render/JMaterialFactory.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"

J_EDITOR_BEGIN

class JAssetManager
{
public:
	struct MaterialBundle
	{
		std::shared_ptr<Engine::JMaterial> material;
	};

	struct ImportedSceneNode
	{
		std::string name;
		Engine::JSceneTransformData transform;
		std::shared_ptr<Engine::JMesh> mesh;
	};

	struct ImportedScene
	{
		std::vector<ImportedSceneNode> nodes;
		std::vector<std::shared_ptr<MaterialBundle>> materialBundles;
	};

	explicit JAssetManager(Engine::JMaterialFactory* materialFactory = nullptr);

	void Initialize(Engine::JMaterialFactory* materialFactory);
	void SetResourceRoot(const std::filesystem::path& resourceRoot);
	void Clear();

	std::shared_ptr<MaterialBundle> AcquireMaterialBundle(const Engine::JSceneMaterialData& materialData);
	std::shared_ptr<Engine::JMesh> AcquireMesh(const Engine::JSceneMeshData& meshData);
	const std::vector<std::shared_ptr<MaterialBundle>>* GetImportedMaterialBundles(const Engine::JSceneMeshData& meshData) const;
	const ImportedScene* GetImportedScene(const Engine::JSceneMeshData& meshData) const;
	bool HasMaterialBundle(const Engine::JSceneMaterialData& materialData) const;
	bool HasMesh(const Engine::JSceneMeshData& meshData) const;
	bool HasTexture(const std::string& path) const;

private:
	std::string resolveResourcePath(const std::string& path) const;
	std::string makeMaterialKey(const Engine::JSceneMaterialData& materialData) const;
	std::string makeMeshKey(const Engine::JSceneMeshData& meshData) const;
	std::string makeTextureKey(const std::string& path) const;
	Engine::JSceneMaterialData loadMaterialData(const Engine::JSceneMaterialData& materialData) const;
	std::shared_ptr<Engine::JMesh> loadMeshAsset(const std::string& path) const;

	static std::shared_ptr<Engine::JMesh> adoptMesh(Engine::JMesh* mesh);

	Engine::JMaterialFactory* _materialFactory = nullptr;
	std::filesystem::path _resourceRoot;
	std::unordered_map<size_t, std::weak_ptr<MaterialBundle>> _materialCache;
	std::unordered_map<size_t, std::weak_ptr<Engine::JMesh>> _meshCache;
	std::unordered_map<size_t, std::vector<std::shared_ptr<MaterialBundle>>> _importedMaterialCache;
	std::unordered_map<size_t, ImportedScene> _importedSceneCache;
	std::unordered_set<size_t> _knownTextureKeys;
};

J_EDITOR_END

#endif
