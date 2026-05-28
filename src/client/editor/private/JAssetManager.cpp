#include "client/editor/JAssetManager.h"

#include "client/editor/JFBXLoader.h"

#include <iomanip>
#include <sstream>

J_EDITOR_BEGIN

namespace
{
	template<typename T>
	void append_value(std::ostringstream& oss, const T& value)
	{
		oss << value << ';';
	}
}

JAssetManager::JAssetManager(Engine::JMaterialFactory* materialFactory)
	: _materialFactory(materialFactory)
{
}

void JAssetManager::Initialize(Engine::JMaterialFactory* materialFactory)
{
	_materialFactory = materialFactory;
}

void JAssetManager::Clear()
{
	_materialCache.clear();
	_meshCache.clear();
	_importedMaterialCache.clear();
	_knownTextureKeys.clear();
}

std::string JAssetManager::resolveResourcePath(const std::string& path)
{
	const std::filesystem::path sourcePath(path);
	if (sourcePath.is_absolute())
	{
		return sourcePath.string();
	}

	return (std::filesystem::path(get_Engine_Res_Path()) / sourcePath).string();
}

std::string JAssetManager::makeMaterialKey(const Engine::JSceneMaterialData& materialData)
{
	std::ostringstream oss;
	append_value(oss, resolveResourcePath(materialData.shaderPath));
	append_value(oss, materialData.enableAlphaBlend);
	append_value(oss, materialData.constants.enabled);
	append_value(oss, materialData.constants.baseColor.x);
	append_value(oss, materialData.constants.baseColor.y);
	append_value(oss, materialData.constants.baseColor.z);
	append_value(oss, materialData.constants.baseColor.w);
	append_value(oss, materialData.constants.roughness);
	append_value(oss, materialData.constants.metallic);
	for (const Engine::JSceneMaterialTextureData& texture : materialData.textures)
	{
		append_value(oss, texture.name);
		append_value(oss, resolveResourcePath(texture.path));
	}
	return oss.str();
}

std::string JAssetManager::makeMeshKey(const Engine::JSceneMeshData& meshData)
{
	std::ostringstream oss;
	append_value(oss, static_cast<int>(meshData.type));
	append_value(oss, resolveResourcePath(meshData.path));
	append_value(oss, meshData.plane.size);
	append_value(oss, meshData.plane.y);
	return oss.str();
}

std::string JAssetManager::makeTextureKey(const std::string& path)
{
	return resolveResourcePath(path);
}

std::shared_ptr<Engine::JMesh> JAssetManager::adoptMesh(Engine::JMesh* mesh)
{
	return std::shared_ptr<Engine::JMesh>(mesh);
}

std::shared_ptr<JAssetManager::MaterialBundle> JAssetManager::AcquireMaterialBundle(const Engine::JSceneMaterialData& materialData)
{
	if (_materialFactory == nullptr)
	{
		return {};
	}

	const size_t key = std::hash<std::string>{}(makeMaterialKey(materialData));
	const auto cachedIter = _materialCache.find(key);
	if (cachedIter != _materialCache.end())
	{
		if (std::shared_ptr<MaterialBundle> cached = cachedIter->second.lock())
		{
			return cached;
		}
	}

	const std::string shaderPath = resolveResourcePath(materialData.shaderPath);
	std::shared_ptr<Engine::JMaterial> material(_materialFactory->CreateMaterial(shaderPath, materialData.enableAlphaBlend));
	if (!material)
	{
		return {};
	}

	material->SetName(!materialData.name.empty() ? materialData.name : materialData.id);

	auto bundle = std::make_shared<MaterialBundle>();
	bundle->material = material;

	if (materialData.constants.enabled)
	{
		struct MaterialConstants
		{
			JVec4 baseColor;
			float roughness = 0.5f;
			float metallic = 1.0f;
			JVec2 padding = {};
		};

		MaterialConstants constants{};
		constants.baseColor = materialData.constants.baseColor;
		constants.roughness = materialData.constants.roughness;
		constants.metallic = materialData.constants.metallic;

		_materialFactory->SetConstantBufferData(material.get(), "PerMaterial", &constants, sizeof(constants));
	}

	for (const Engine::JSceneMaterialTextureData& textureData : materialData.textures)
	{
		const size_t textureKey = std::hash<std::string>{}(makeTextureKey(textureData.path));
		_knownTextureKeys.insert(textureKey);
		_materialFactory->SetTexturePath(material.get(), textureData.name, resolveResourcePath(textureData.path));
	}

	_materialCache[key] = bundle;
	return bundle;
}

bool JAssetManager::HasMaterialBundle(const Engine::JSceneMaterialData& materialData) const
{
	const size_t key = std::hash<std::string>{}(makeMaterialKey(materialData));
	const auto cachedIter = _materialCache.find(key);
	if (cachedIter == _materialCache.end())
	{
		return false;
	}

	return !cachedIter->second.expired();
}

const std::vector<std::shared_ptr<JAssetManager::MaterialBundle>>* JAssetManager::GetImportedMaterialBundles(const Engine::JSceneMeshData& meshData) const
{
	const size_t key = std::hash<std::string>{}(makeMeshKey(meshData));
	const auto iter = _importedMaterialCache.find(key);
	return iter != _importedMaterialCache.end() ? &iter->second : nullptr;
}

std::shared_ptr<Engine::JMesh> JAssetManager::AcquireMesh(const Engine::JSceneMeshData& meshData)
{
	const size_t key = std::hash<std::string>{}(makeMeshKey(meshData));
	const auto cachedIter = _meshCache.find(key);
	if (cachedIter != _meshCache.end())
	{
		if (std::shared_ptr<Engine::JMesh> cached = cachedIter->second.lock())
		{
			return cached;
		}
	}

	std::shared_ptr<Engine::JMesh> mesh;
	if (meshData.type == Engine::JSceneMeshType::Plane)
	{
		const float size = meshData.plane.size;
		const float y = meshData.plane.y;

		std::vector<float> positions =
		{
			-size, y, -size, 1.0f,
			 size, y, -size, 1.0f,
			 size, y,  size, 1.0f,
			-size, y,  size, 1.0f,
		};

		std::vector<uint32> indices = { 0, 1, 2, 0, 2, 3 };

		mesh = std::make_shared<Engine::JMesh>();
		mesh->SetPositions(std::move(positions));
		mesh->SetIndices(std::move(indices));
	}
	else
	{
		const std::string meshPath = resolveResourcePath(meshData.path);
		JFBXLoader fbxLoader;
		std::vector<JFBXLoader::ImportedMaterial> importedMaterials;
		mesh = adoptMesh(fbxLoader.LoadFBX(meshPath.c_str(), &importedMaterials));
		if (_materialFactory != nullptr && !importedMaterials.empty())
		{
			std::vector<std::shared_ptr<MaterialBundle>> materialBundles;
			materialBundles.reserve(importedMaterials.size());
			const std::filesystem::path meshDirectory = std::filesystem::path(meshPath).parent_path();
			for (uint32 materialIndex = 0; materialIndex < importedMaterials.size(); ++materialIndex)
			{
				const JFBXLoader::ImportedMaterial& imported = importedMaterials[materialIndex];

				Engine::JSceneMaterialData materialData;
				materialData.id = meshData.id + "_fbx_mat_" + std::to_string(materialIndex);
				materialData.name = !imported.name.empty() ? imported.name : materialData.id;
				materialData.shaderPath = "shader/base.hlsl";
				materialData.enableAlphaBlend = false;
				materialData.constants.enabled = true;
				materialData.constants.baseColor = imported.baseColor;
				materialData.constants.roughness = 0.5f;
				materialData.constants.metallic = 0.0f;

				auto resolveImportedTexturePath = [&meshDirectory](const std::string& texturePath) -> std::string
				{
					if (texturePath.empty())
					{
						return {};
					}

					const std::filesystem::path sourcePath(texturePath);
					if (sourcePath.is_absolute())
					{
						return sourcePath.string();
					}

					const std::filesystem::path meshRelativePath = meshDirectory / sourcePath;
					if (std::filesystem::exists(meshRelativePath))
					{
						return meshRelativePath.string();
					}

					const std::filesystem::path fileName = sourcePath.filename();
					const std::filesystem::path engineTexturePath = std::filesystem::path(get_Engine_Res_Path()) / "texture" / fileName;
					if (std::filesystem::exists(engineTexturePath))
					{
						return engineTexturePath.string();
					}

					const std::filesystem::path engineTexturesPath = std::filesystem::path(get_Engine_Res_Path()) / "textures" / fileName;
					if (std::filesystem::exists(engineTexturesPath))
					{
						return engineTexturesPath.string();
					}

					return (std::filesystem::path(get_Engine_Res_Path()) / sourcePath).string();
				};

				const std::string diffuseTexturePath = resolveImportedTexturePath(imported.diffuseTexturePath);
				if (!diffuseTexturePath.empty())
				{
					materialData.textures.push_back({ "BaseTexture", diffuseTexturePath });
				}

				const std::string normalTexturePath = resolveImportedTexturePath(imported.normalTexturePath);
				if (!normalTexturePath.empty())
				{
					materialData.textures.push_back({ "NormalTexture", normalTexturePath });
				}

				const std::string specularTexturePath = resolveImportedTexturePath(imported.specularTexturePath);
				if (!specularTexturePath.empty())
				{
					materialData.textures.push_back({ "SpecularTexture", specularTexturePath });
				}

				std::shared_ptr<MaterialBundle> materialBundle = AcquireMaterialBundle(materialData);
				if (materialBundle && materialBundle->material)
				{
					materialBundles.push_back(materialBundle);
				}
			}

			if (!materialBundles.empty())
			{
				_importedMaterialCache[key] = std::move(materialBundles);
			}
		}
	}

	if (!mesh)
	{
		return {};
	}

	_meshCache[key] = mesh;
	return mesh;
}

bool JAssetManager::HasMesh(const Engine::JSceneMeshData& meshData) const
{
	const size_t key = std::hash<std::string>{}(makeMeshKey(meshData));
	const auto cachedIter = _meshCache.find(key);
	if (cachedIter == _meshCache.end())
	{
		return false;
	}

	return !cachedIter->second.expired();
}

bool JAssetManager::HasTexture(const std::string& path) const
{
	const size_t key = std::hash<std::string>{}(makeTextureKey(path));
	return _knownTextureKeys.find(key) != _knownTextureKeys.end();
}

J_EDITOR_END
