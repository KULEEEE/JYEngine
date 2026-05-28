#include "client/editor/JAssetManager.h"

#include "client/editor/JFBXLoader.h"
#include "third_party/nlohmann/json.hpp"

#include <iomanip>
#include <fstream>
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
	, _resourceRoot(get_Engine_Res_Path())
{
}

void JAssetManager::Initialize(Engine::JMaterialFactory* materialFactory)
{
	_materialFactory = materialFactory;
}

void JAssetManager::SetResourceRoot(const std::filesystem::path& resourceRoot)
{
	_resourceRoot = resourceRoot.empty() ? std::filesystem::path(get_Engine_Res_Path()) : resourceRoot;
}

void JAssetManager::Clear()
{
	_materialCache.clear();
	_meshCache.clear();
	_importedMaterialCache.clear();
	_importedSceneCache.clear();
	_knownTextureKeys.clear();
}

std::string JAssetManager::resolveResourcePath(const std::string& path) const
{
	const std::filesystem::path sourcePath(path);
	if (sourcePath.is_absolute())
	{
		return sourcePath.string();
	}

	return (_resourceRoot / sourcePath).string();
}

std::string JAssetManager::makeMaterialKey(const Engine::JSceneMaterialData& materialData) const
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

std::string JAssetManager::makeMeshKey(const Engine::JSceneMeshData& meshData) const
{
	std::ostringstream oss;
	append_value(oss, static_cast<int>(meshData.type));
	append_value(oss, resolveResourcePath(meshData.path));
	append_value(oss, meshData.plane.size);
	append_value(oss, meshData.plane.y);
	return oss.str();
}

std::string JAssetManager::makeTextureKey(const std::string& path) const
{
	return resolveResourcePath(path);
}

Engine::JSceneMaterialData JAssetManager::loadMaterialData(const Engine::JSceneMaterialData& materialData) const
{
	if (materialData.path.empty())
	{
		return materialData;
	}

	std::ifstream stream(resolveResourcePath(materialData.path));
	if (!stream)
	{
		return materialData;
	}

	nlohmann::json root;
	stream >> root;

	Engine::JSceneMaterialData loaded = materialData;
	loaded.path.clear();
	loaded.name = root.value("name", loaded.name);
	loaded.shaderPath = root.value("shaderPath", loaded.shaderPath);
	loaded.enableAlphaBlend = root.value("enableAlphaBlend", loaded.enableAlphaBlend);
	if (root.contains("constants") && root.at("constants").is_object())
	{
		const nlohmann::json& constants = root.at("constants");
		loaded.constants.enabled = constants.value("enabled", loaded.constants.enabled);
		if (constants.contains("baseColor") && constants.at("baseColor").is_array() && constants.at("baseColor").size() >= 4)
		{
			loaded.constants.baseColor = {
				constants.at("baseColor").at(0).get<float>(),
				constants.at("baseColor").at(1).get<float>(),
				constants.at("baseColor").at(2).get<float>(),
				constants.at("baseColor").at(3).get<float>()
			};
		}
		loaded.constants.roughness = constants.value("roughness", loaded.constants.roughness);
		loaded.constants.metallic = constants.value("metallic", loaded.constants.metallic);
	}
	if (root.contains("textures") && root.at("textures").is_array())
	{
		loaded.textures.clear();
		for (const nlohmann::json& textureValue : root.at("textures"))
		{
			loaded.textures.push_back({
				textureValue.value("name", ""),
				textureValue.value("path", "")
			});
		}
	}
	return loaded;
}

std::shared_ptr<Engine::JMesh> JAssetManager::loadMeshAsset(const std::string& path) const
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
	{
		return {};
	}

	char magic[8] = {};
	uint32 version = 0;
	stream.read(magic, sizeof(magic));
	stream.read(reinterpret_cast<char*>(&version), sizeof(version));
	if (std::string(magic, magic + 7) != "JYMESH1" || version != 1)
	{
		return {};
	}

	auto readFloatVector = [&stream]()
	{
		uint64 count = 0;
		stream.read(reinterpret_cast<char*>(&count), sizeof(count));
		std::vector<float> values(static_cast<size_t>(count));
		if (count > 0)
		{
			stream.read(reinterpret_cast<char*>(values.data()), sizeof(float) * values.size());
		}
		return values;
	};
	auto readUIntVector = [&stream]()
	{
		uint64 count = 0;
		stream.read(reinterpret_cast<char*>(&count), sizeof(count));
		std::vector<uint32> values(static_cast<size_t>(count));
		if (count > 0)
		{
			stream.read(reinterpret_cast<char*>(values.data()), sizeof(uint32) * values.size());
		}
		return values;
	};

	auto mesh = std::make_shared<Engine::JMesh>();
	mesh->SetPositions(readFloatVector());
	mesh->SetNormals(readFloatVector());
	mesh->SetTexcoords(readFloatVector(), 0);
	mesh->SetColors(readFloatVector());
	mesh->SetTangents(readFloatVector());
	mesh->SetBitangents(readFloatVector());
	mesh->SetIndices(readUIntVector());

	uint64 subMeshCount = 0;
	stream.read(reinterpret_cast<char*>(&subMeshCount), sizeof(subMeshCount));
	std::vector<Engine::JMesh::SubMeshInfo> subMeshes(static_cast<size_t>(subMeshCount));
	if (subMeshCount > 0)
	{
		stream.read(reinterpret_cast<char*>(subMeshes.data()), sizeof(Engine::JMesh::SubMeshInfo) * subMeshes.size());
	}
	mesh->SetSubMeshes(std::move(subMeshes));
	return stream ? mesh : std::shared_ptr<Engine::JMesh>{};
}

std::shared_ptr<Engine::JMesh> JAssetManager::adoptMesh(Engine::JMesh* mesh)
{
	return std::shared_ptr<Engine::JMesh>(mesh);
}

std::shared_ptr<JAssetManager::MaterialBundle> JAssetManager::AcquireMaterialBundle(const Engine::JSceneMaterialData& materialData)
{
	const Engine::JSceneMaterialData resolvedMaterialData = loadMaterialData(materialData);
	if (_materialFactory == nullptr)
	{
		return {};
	}

	const size_t key = std::hash<std::string>{}(makeMaterialKey(resolvedMaterialData));
	const auto cachedIter = _materialCache.find(key);
	if (cachedIter != _materialCache.end())
	{
		if (std::shared_ptr<MaterialBundle> cached = cachedIter->second.lock())
		{
			return cached;
		}
	}

	const std::string shaderPath = resolveResourcePath(resolvedMaterialData.shaderPath);
	std::shared_ptr<Engine::JMaterial> material(_materialFactory->CreateMaterial(shaderPath, resolvedMaterialData.enableAlphaBlend));
	if (!material)
	{
		return {};
	}

	material->SetName(!resolvedMaterialData.name.empty() ? resolvedMaterialData.name : resolvedMaterialData.id);

	auto bundle = std::make_shared<MaterialBundle>();
	bundle->material = material;

	if (resolvedMaterialData.constants.enabled)
	{
		struct MaterialConstants
		{
			JVec4 baseColor;
			float roughness = 0.5f;
			float metallic = 1.0f;
			JVec2 padding = {};
		};

		MaterialConstants constants{};
		constants.baseColor = resolvedMaterialData.constants.baseColor;
		constants.roughness = resolvedMaterialData.constants.roughness;
		constants.metallic = resolvedMaterialData.constants.metallic;

		_materialFactory->SetConstantBufferData(material.get(), "PerMaterial", &constants, sizeof(constants));
	}

	for (const Engine::JSceneMaterialTextureData& textureData : resolvedMaterialData.textures)
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

const JAssetManager::ImportedScene* JAssetManager::GetImportedScene(const Engine::JSceneMeshData& meshData) const
{
	const size_t key = std::hash<std::string>{}(makeMeshKey(meshData));
	const auto iter = _importedSceneCache.find(key);
	return iter != _importedSceneCache.end() ? &iter->second : nullptr;
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
		if (std::filesystem::path(meshPath).extension() == ".jmesh")
		{
			mesh = loadMeshAsset(meshPath);
			if (!mesh)
			{
				return {};
			}

			_meshCache[key] = mesh;
			return mesh;
		}

		JFBXLoader fbxLoader;
		JFBXLoader::ImportedScene importedScene;
		const bool importedSceneLoaded = fbxLoader.LoadFBXScene(meshPath.c_str(), importedScene);
		std::vector<JFBXLoader::ImportedMaterial> importedMaterials;
		if (importedSceneLoaded)
		{
			importedMaterials = importedScene.materials;
		}
		else
		{
			mesh = adoptMesh(fbxLoader.LoadFBX(meshPath.c_str(), &importedMaterials));
		}
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
				materialData.shaderPath = "shader/gbuffer_albedo.hlsl";
				materialData.enableAlphaBlend = false;
				materialData.constants.enabled = true;
				materialData.constants.baseColor = imported.baseColor;
				materialData.constants.roughness = 0.5f;
				materialData.constants.metallic = 0.0f;

				const std::filesystem::path resourceRoot = _resourceRoot;
				auto resolveImportedTexturePath = [&meshDirectory, resourceRoot](const std::string& texturePath) -> std::string
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
					const std::filesystem::path engineTexturePath = resourceRoot / "textures" / fileName;
					if (std::filesystem::exists(engineTexturePath))
					{
						return engineTexturePath.string();
					}

					const std::filesystem::path engineTexturesPath = resourceRoot / "texture" / fileName;
					if (std::filesystem::exists(engineTexturesPath))
					{
						return engineTexturesPath.string();
					}

					return (resourceRoot / sourcePath).string();
				};

				auto findSiblingTexturePath = [&resolveImportedTexturePath](const std::string& sourceTexturePath, const std::string& sourceToken, const std::string& targetToken) -> std::string
				{
					if (sourceTexturePath.empty())
					{
						return {};
					}

					std::filesystem::path sourcePath(sourceTexturePath);
					std::string fileName = sourcePath.filename().string();
					const size_t tokenPos = fileName.find(sourceToken);
					if (tokenPos == std::string::npos)
					{
						return {};
					}

					fileName.replace(tokenPos, sourceToken.size(), targetToken);
					sourcePath.replace_filename(fileName);
					const std::string resolvedPath = resolveImportedTexturePath(sourcePath.string());
					if (std::filesystem::exists(resolvedPath))
					{
						return resolvedPath;
					}

					for (const std::string& colorToken : { "_blue", "_green", "_red" })
					{
						std::filesystem::path colorAgnosticPath(sourceTexturePath);
						std::string colorAgnosticFileName = colorAgnosticPath.filename().string();
						const size_t colorPos = colorAgnosticFileName.find(colorToken + "_" + sourceToken);
						if (colorPos == std::string::npos)
						{
							continue;
						}

						colorAgnosticFileName.erase(colorPos, colorToken.size());
						const size_t colorAgnosticTokenPos = colorAgnosticFileName.find(sourceToken);
						if (colorAgnosticTokenPos == std::string::npos)
						{
							continue;
						}

						colorAgnosticFileName.replace(colorAgnosticTokenPos, sourceToken.size(), targetToken);
						colorAgnosticPath.replace_filename(colorAgnosticFileName);
						const std::string colorAgnosticResolvedPath = resolveImportedTexturePath(colorAgnosticPath.string());
						if (std::filesystem::exists(colorAgnosticResolvedPath))
						{
							return colorAgnosticResolvedPath;
						}
					}

					return {};
				};

				const std::string diffuseTexturePath = resolveImportedTexturePath(imported.diffuseTexturePath);
				if (!diffuseTexturePath.empty())
				{
					materialData.textures.push_back({ "BaseTexture", diffuseTexturePath });
					materialData.constants.baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
				}

				std::string normalTexturePath = resolveImportedTexturePath(imported.normalTexturePath);
				if (normalTexturePath.empty())
				{
					normalTexturePath = findSiblingTexturePath(imported.diffuseTexturePath, "BaseColor", "Normal");
				}
				if (!normalTexturePath.empty())
				{
					materialData.textures.push_back({ "NormalTexture", normalTexturePath });
				}

				const std::string roughnessTexturePath = findSiblingTexturePath(imported.diffuseTexturePath, "BaseColor", "Roughness");
				if (!roughnessTexturePath.empty())
				{
					materialData.textures.push_back({ "RoughnessTexture", roughnessTexturePath });
				}

				const std::string metallicTexturePath = findSiblingTexturePath(imported.diffuseTexturePath, "BaseColor", "Metalness");
				if (!metallicTexturePath.empty())
				{
					materialData.textures.push_back({ "MetallicTexture", metallicTexturePath });
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

		if (importedSceneLoaded)
		{
			ImportedScene cachedScene;
			cachedScene.materialBundles = _importedMaterialCache[key];
			cachedScene.nodes.reserve(importedScene.nodes.size());
			for (JFBXLoader::ImportedNode& importedNode : importedScene.nodes)
			{
				std::shared_ptr<Engine::JMesh> nodeMesh = adoptMesh(importedNode.mesh);
				importedNode.mesh = nullptr;
				if (!nodeMesh)
				{
					continue;
				}

				ImportedSceneNode cachedNode;
				cachedNode.name = importedNode.name;
				cachedNode.transform.translation = importedNode.translation;
				cachedNode.transform.rotation = importedNode.rotation;
				cachedNode.transform.scale = importedNode.scale;
				cachedNode.mesh = nodeMesh;
				cachedScene.nodes.push_back(cachedNode);
			}

			if (!cachedScene.nodes.empty())
			{
				mesh = cachedScene.nodes.front().mesh;
				_importedSceneCache[key] = std::move(cachedScene);
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
