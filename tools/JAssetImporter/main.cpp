#include "client/editor/JFBXLoader.h"
#include "third_party/nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
	std::string sanitizeName(std::string value)
	{
		for (char& ch : value)
		{
			if (!std::isalnum(static_cast<unsigned char>(ch)))
			{
				ch = '_';
			}
		}
		value.erase(std::unique(value.begin(), value.end(), [](char a, char b) { return a == '_' && b == '_'; }), value.end());
		while (!value.empty() && value.front() == '_') value.erase(value.begin());
		while (!value.empty() && value.back() == '_') value.pop_back();
		return value.empty() ? "asset" : value;
	}

	void writeFloatVector(std::ofstream& stream, const std::vector<float>& values)
	{
		const uint64 count = static_cast<uint64>(values.size());
		stream.write(reinterpret_cast<const char*>(&count), sizeof(count));
		if (!values.empty())
		{
			stream.write(reinterpret_cast<const char*>(values.data()), sizeof(float) * values.size());
		}
	}

	void writeUIntVector(std::ofstream& stream, const std::vector<uint32>& values)
	{
		const uint64 count = static_cast<uint64>(values.size());
		stream.write(reinterpret_cast<const char*>(&count), sizeof(count));
		if (!values.empty())
		{
			stream.write(reinterpret_cast<const char*>(values.data()), sizeof(uint32) * values.size());
		}
	}

	bool writeMesh(const std::filesystem::path& path, const J::Engine::JMesh& mesh, const std::vector<uint32>& materialIndexRemap)
	{
		std::ofstream stream(path, std::ios::binary);
		if (!stream)
		{
			return false;
		}

		const char magic[8] = { 'J', 'Y', 'M', 'E', 'S', 'H', '1', '\0' };
		const uint32 version = 1;
		stream.write(magic, sizeof(magic));
		stream.write(reinterpret_cast<const char*>(&version), sizeof(version));
		writeFloatVector(stream, mesh.GetPositions());
		writeFloatVector(stream, mesh.GetNormals());
		writeFloatVector(stream, mesh.GetTexcoords(0));
		writeFloatVector(stream, mesh.GetColors());
		writeFloatVector(stream, mesh.GetTangents());
		writeFloatVector(stream, mesh.GetBitangents());
		writeUIntVector(stream, mesh.GetIndices());

		std::vector<J::Engine::JMesh::SubMeshInfo> subMeshes = mesh.GetSubMeshInfos();
		for (J::Engine::JMesh::SubMeshInfo& subMesh : subMeshes)
		{
			if (subMesh.materialIndex < materialIndexRemap.size())
			{
				subMesh.materialIndex = materialIndexRemap[subMesh.materialIndex];
			}
			else
			{
				subMesh.materialIndex = 0;
			}
		}
		const uint64 subMeshCount = static_cast<uint64>(subMeshes.size());
		stream.write(reinterpret_cast<const char*>(&subMeshCount), sizeof(subMeshCount));
		if (!subMeshes.empty())
		{
			stream.write(reinterpret_cast<const char*>(subMeshes.data()), sizeof(J::Engine::JMesh::SubMeshInfo) * subMeshes.size());
		}
		return static_cast<bool>(stream);
	}

	std::filesystem::path resolveSourceTexture(const std::filesystem::path& fbxDirectory, const std::string& texturePath)
	{
		if (texturePath.empty())
		{
			return {};
		}

		const std::filesystem::path sourcePath(texturePath);
		if (sourcePath.is_absolute() && std::filesystem::exists(sourcePath))
		{
			return sourcePath;
		}

		const std::filesystem::path fbxRelativePath = fbxDirectory / sourcePath;
		if (std::filesystem::exists(fbxRelativePath))
		{
			return fbxRelativePath;
		}

		const std::filesystem::path fileName = sourcePath.filename();
		for (const std::filesystem::path& folder :
			{
				fbxDirectory,
				fbxDirectory / "textures",
				fbxDirectory / "texture",
				fbxDirectory.parent_path() / "textures",
				fbxDirectory.parent_path() / "texture",
				std::filesystem::current_path() / "res" / "textures",
				std::filesystem::current_path() / "res" / "texture"
			})
		{
			const std::filesystem::path candidate = folder / fileName;
			if (std::filesystem::exists(candidate))
			{
				return candidate;
			}
		}

		return {};
	}

	std::filesystem::path findSiblingTexture(const std::filesystem::path& fbxDirectory, const std::string& baseTexturePath, const std::string& sourceToken, const std::string& targetToken)
	{
		if (baseTexturePath.empty())
		{
			return {};
		}

		std::filesystem::path sourcePath(baseTexturePath);
		std::string fileName = sourcePath.filename().string();
		const size_t tokenPos = fileName.find(sourceToken);
		if (tokenPos == std::string::npos)
		{
			return {};
		}

		fileName.replace(tokenPos, sourceToken.size(), targetToken);
		sourcePath.replace_filename(fileName);
		std::filesystem::path resolvedPath = resolveSourceTexture(fbxDirectory, sourcePath.string());
		if (!resolvedPath.empty())
		{
			return resolvedPath;
		}

		for (const std::string& colorToken : { "_blue", "_green", "_red" })
		{
			std::filesystem::path colorAgnosticPath(baseTexturePath);
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
			resolvedPath = resolveSourceTexture(fbxDirectory, colorAgnosticPath.string());
			if (!resolvedPath.empty())
			{
				return resolvedPath;
			}
		}

		return {};
	}

	std::string copyTexture(const std::filesystem::path& sourcePath, const std::filesystem::path& projectRoot)
	{
		if (sourcePath.empty() || !std::filesystem::exists(sourcePath))
		{
			return {};
		}

		const std::filesystem::path textureDir = projectRoot / "textures";
		std::filesystem::create_directories(textureDir);
		const std::filesystem::path destination = textureDir / sourcePath.filename();
		std::error_code error;
		std::filesystem::copy_file(sourcePath, destination, std::filesystem::copy_options::overwrite_existing, error);
		return error ? std::string{} : ("textures/" + destination.filename().string());
	}

	std::string makeMaterialKey(const JFBXLoader::ImportedMaterial& material)
	{
		std::ostringstream stream;
		stream << material.name << '|'
			<< material.diffuseTexturePath << '|'
			<< material.normalTexturePath << '|'
			<< material.specularTexturePath << '|'
			<< material.baseColor.x << ','
			<< material.baseColor.y << ','
			<< material.baseColor.z << ','
			<< material.baseColor.w;
		return stream.str();
	}

	void copyShaderFiles(const std::filesystem::path& projectRoot)
	{
		std::filesystem::path sourceDir = std::filesystem::current_path() / "tools" / "JAssetImporter" / "shader";
		if (!std::filesystem::exists(sourceDir))
		{
			sourceDir = std::filesystem::current_path() / "res" / "shader";
		}
		if (!std::filesystem::exists(sourceDir))
		{
			sourceDir = get_Engine_Executable_Path() / "res" / "shader";
		}
		const std::filesystem::path destinationDir = projectRoot / "shader";
		if (!std::filesystem::exists(sourceDir))
		{
			std::cerr << "Warning: shader source directory not found: " << sourceDir.string() << "\n";
			return;
		}

		std::filesystem::create_directories(destinationDir);
		for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(sourceDir))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}

			const std::filesystem::path destination = destinationDir / entry.path().filename();
			std::error_code error;
			std::filesystem::copy_file(entry.path(), destination, std::filesystem::copy_options::overwrite_existing, error);
			if (error)
			{
				std::cerr << "Warning: failed to copy shader: " << entry.path().string() << "\n";
			}
		}
	}
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::cerr << "Usage: JAssetImporter <source.fbx> <projectDir>\n";
		return 1;
	}

	const std::filesystem::path fbxPath = std::filesystem::absolute(argv[1]);
	const std::filesystem::path projectRoot = std::filesystem::absolute(argv[2]);
	const std::filesystem::path fbxDirectory = fbxPath.parent_path();

	std::filesystem::create_directories(projectRoot / "scenes");
	std::filesystem::create_directories(projectRoot / "meshes");
	std::filesystem::create_directories(projectRoot / "materials");
	std::filesystem::create_directories(projectRoot / "textures");
	std::filesystem::create_directories(projectRoot / "shader");
	copyShaderFiles(projectRoot);

	JFBXLoader loader;
	JFBXLoader::ImportedScene importedScene;
	if (!loader.LoadFBXScene(fbxPath.string().c_str(), importedScene))
	{
		std::cerr << "Failed to import FBX: " << fbxPath.string() << "\n";
		return 2;
	}
	std::vector<std::unique_ptr<J::Engine::JMesh>> importedMeshOwners;
	importedMeshOwners.reserve(importedScene.nodes.size());
	for (JFBXLoader::ImportedNode& node : importedScene.nodes)
	{
		importedMeshOwners.emplace_back(node.mesh);
	}

	const std::string projectName = sanitizeName(fbxPath.stem().string());
	std::vector<std::string> materialIDs;
	std::vector<uint32> materialIndexRemap(importedScene.materials.size(), 0);
	std::unordered_map<std::string, uint32> uniqueMaterialIndexByKey;
	nlohmann::json materialsJson = nlohmann::json::array();
	for (uint32 i = 0; i < importedScene.materials.size(); ++i)
	{
		const JFBXLoader::ImportedMaterial& importedMaterial = importedScene.materials[i];
		const std::string materialKey = makeMaterialKey(importedMaterial);
		const auto materialIter = uniqueMaterialIndexByKey.find(materialKey);
		if (materialIter != uniqueMaterialIndexByKey.end())
		{
			materialIndexRemap[i] = materialIter->second;
			continue;
		}

		const uint32 uniqueMaterialIndex = static_cast<uint32>(materialIDs.size());
		uniqueMaterialIndexByKey[materialKey] = uniqueMaterialIndex;
		materialIndexRemap[i] = uniqueMaterialIndex;

		const std::string materialName = sanitizeName(!importedMaterial.name.empty() ? importedMaterial.name : ("material_" + std::to_string(i)));
		const std::string materialID = "mat_" + materialName + "_" + std::to_string(uniqueMaterialIndex);
		const std::filesystem::path materialPath = projectRoot / "materials" / (materialID + ".jmaterial");
		materialIDs.push_back(materialID);

		nlohmann::json materialJson;
		materialJson["name"] = importedMaterial.name.empty() ? materialID : importedMaterial.name;
		materialJson["shaderPath"] = "shader/gbuffer_albedo.hlsl";
		materialJson["enableAlphaBlend"] = false;
		materialJson["constants"] =
		{
			{ "enabled", true },
			{ "baseColor", { 1.0f, 1.0f, 1.0f, 1.0f } },
			{ "roughness", 0.5f },
			{ "metallic", 0.0f },
		};
		materialJson["textures"] = nlohmann::json::array();

		const std::filesystem::path baseTexture = resolveSourceTexture(fbxDirectory, importedMaterial.diffuseTexturePath);
		const std::string baseTexturePath = copyTexture(baseTexture, projectRoot);
		if (!baseTexturePath.empty())
		{
			materialJson["textures"].push_back({ { "name", "BaseTexture" }, { "path", baseTexturePath } });
		}

		const std::filesystem::path normalTexture = !importedMaterial.normalTexturePath.empty()
			? resolveSourceTexture(fbxDirectory, importedMaterial.normalTexturePath)
			: findSiblingTexture(fbxDirectory, importedMaterial.diffuseTexturePath, "BaseColor", "Normal");
		const std::string normalTexturePath = copyTexture(normalTexture, projectRoot);
		if (!normalTexturePath.empty())
		{
			materialJson["textures"].push_back({ { "name", "NormalTexture" }, { "path", normalTexturePath } });
		}

		const std::string roughnessTexturePath = copyTexture(findSiblingTexture(fbxDirectory, importedMaterial.diffuseTexturePath, "BaseColor", "Roughness"), projectRoot);
		if (!roughnessTexturePath.empty())
		{
			materialJson["textures"].push_back({ { "name", "RoughnessTexture" }, { "path", roughnessTexturePath } });
		}

		const std::string metallicTexturePath = copyTexture(findSiblingTexture(fbxDirectory, importedMaterial.diffuseTexturePath, "BaseColor", "Metalness"), projectRoot);
		if (!metallicTexturePath.empty())
		{
			materialJson["textures"].push_back({ { "name", "MetallicTexture" }, { "path", metallicTexturePath } });
		}

		std::ofstream materialStream(materialPath);
		materialStream << materialJson.dump(2);
		materialsJson.push_back({ { "id", materialID }, { "path", "materials/" + materialPath.filename().string() } });
	}

	nlohmann::json meshesJson = nlohmann::json::array();
	nlohmann::json entitiesJson = nlohmann::json::array();
	for (uint32 i = 0; i < importedScene.nodes.size(); ++i)
	{
		const JFBXLoader::ImportedNode& node = importedScene.nodes[i];
		if (node.mesh == nullptr)
		{
			continue;
		}

		const std::string nodeName = sanitizeName(!node.name.empty() ? node.name : ("node_" + std::to_string(i)));
		const std::string meshID = "mesh_" + nodeName + "_" + std::to_string(i);
		const std::filesystem::path meshPath = projectRoot / "meshes" / (meshID + ".jmesh");
		if (!writeMesh(meshPath, *node.mesh, materialIndexRemap))
		{
			std::cerr << "Failed to write mesh: " << meshPath.string() << "\n";
			return 3;
		}

		meshesJson.push_back({ { "id", meshID }, { "name", node.name }, { "type", "External" }, { "path", "meshes/" + meshPath.filename().string() } });
		entitiesJson.push_back(
		{
			{ "stableID", "ent_" + meshID },
			{ "name", node.name.empty() ? meshID : node.name },
			{ "active", true },
			{ "transform", { { "translation", { 0.0f, 0.0f, 0.0f } }, { "rotation", { 0.0f, 0.0f, 0.0f } }, { "scale", { 1.0f, 1.0f, 1.0f } } } },
			{ "renderObjectComponent",
				{
					{ "active", true },
					{ "visible", true },
					{ "transparent", false },
					{ "meshID", meshID },
					{ "materialID", materialIDs.empty() ? "" : materialIDs.front() },
					{ "subMeshMaterialIDs", materialIDs }
				}
			}
		});
	}

	entitiesJson.push_back(
	{
		{ "stableID", "ent_camera" },
		{ "name", "Camera" },
		{ "active", true },
		{ "transform", { { "translation", { 0.0f, 4.0f, -18.0f } }, { "rotation", { 0.0f, 0.0f, 0.0f } }, { "scale", { 1.0f, 1.0f, 1.0f } } } },
		{ "camera", { { "active", true }, { "primary", true }, { "nearP", 0.1f }, { "farP", 2000.0f } } }
	});
	entitiesJson.push_back(
	{
		{ "stableID", "ent_key_light" },
		{ "name", "Key Light" },
		{ "active", true },
		{ "transform", { { "translation", { 0.0f, 12.0f, -4.0f } }, { "rotation", { 0.0f, 0.0f, 0.0f } }, { "scale", { 1.0f, 1.0f, 1.0f } } } },
		{ "light", { { "active", true }, { "color", { 1.0f, 0.95f, 0.85f } }, { "intensity", 6.0f } } }
	});

	nlohmann::json sceneJson;
	sceneJson["id"] = projectName + "_scene";
	sceneJson["name"] = projectName;
	sceneJson["version"] = 1;
	sceneJson["materials"] = materialsJson;
	sceneJson["meshes"] = meshesJson;
	sceneJson["entities"] = entitiesJson;

	std::ofstream sceneStream(projectRoot / "scenes" / "main.scene.json");
	sceneStream << sceneJson.dump(2);

	nlohmann::json projectJson;
	projectJson["name"] = projectName;
	projectJson["startupScene"] = "scenes/main.scene.json";
	std::ofstream projectStream(projectRoot / "project.json");
	projectStream << projectJson.dump(2);

	std::cout << "Imported " << importedScene.nodes.size() << " meshes, "
		<< materialIDs.size() << " unique materials (" << importedScene.materials.size()
		<< " FBX material slots) into " << projectRoot.string() << "\n";
	return 0;
}
