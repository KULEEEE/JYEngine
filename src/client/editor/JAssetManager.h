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

	explicit JAssetManager(Engine::JMaterialFactory* materialFactory = nullptr);

	void Initialize(Engine::JMaterialFactory* materialFactory);
	void Clear();

	std::shared_ptr<MaterialBundle> AcquireMaterialBundle(const Engine::JSceneMaterialData& materialData);
	std::shared_ptr<Engine::JMesh> AcquireMesh(const Engine::JSceneMeshData& meshData);
	bool HasMaterialBundle(const Engine::JSceneMaterialData& materialData) const;
	bool HasMesh(const Engine::JSceneMeshData& meshData) const;
	bool HasTexture(const std::string& path) const;

private:
	static std::string resolveResourcePath(const std::string& path);
	static std::string makeMaterialKey(const Engine::JSceneMaterialData& materialData);
	static std::string makeMeshKey(const Engine::JSceneMeshData& meshData);
	static std::string makeTextureKey(const std::string& path);

	static std::shared_ptr<Engine::JMesh> adoptMesh(Engine::JMesh* mesh);

	Engine::JMaterialFactory* _materialFactory = nullptr;
	std::unordered_map<size_t, std::weak_ptr<MaterialBundle>> _materialCache;
	std::unordered_map<size_t, std::weak_ptr<Engine::JMesh>> _meshCache;
	std::unordered_set<size_t> _knownTextureKeys;
};

J_EDITOR_END

#endif
