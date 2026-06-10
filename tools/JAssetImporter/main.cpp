#include "client/editor/JFBXLoader.h"
#include "engine/asset/JTextureFile.h"
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

	constexpr uint32 makeFourCC(char a, char b, char c, char d)
	{
		return static_cast<uint32>(static_cast<uint8>(a))
			| (static_cast<uint32>(static_cast<uint8>(b)) << 8)
			| (static_cast<uint32>(static_cast<uint8>(c)) << 16)
			| (static_cast<uint32>(static_cast<uint8>(d)) << 24);
	}

	struct DDSPixelFormat
	{
		uint32 size;
		uint32 flags;
		uint32 fourCC;
		uint32 rgbBitCount;
		uint32 rBitMask;
		uint32 gBitMask;
		uint32 bBitMask;
		uint32 aBitMask;
	};

	struct DDSHeader
	{
		uint32 size;
		uint32 flags;
		uint32 height;
		uint32 width;
		uint32 pitchOrLinearSize;
		uint32 depth;
		uint32 mipMapCount;
		uint32 reserved1[11];
		DDSPixelFormat ddspf;
		uint32 caps;
		uint32 caps2;
		uint32 caps3;
		uint32 caps4;
		uint32 reserved2;
	};

	struct DDSHeaderDXT10
	{
		uint32 dxgiFormat;
		uint32 resourceDimension;
		uint32 miscFlag;
		uint32 arraySize;
		uint32 miscFlags2;
	};

	// DDS를 엔진 전용 .jtex 컨테이너로 굽는다. 레거시 fourCC/DX10 헤더 같은 소스 포맷의
	// 변형 처리는 전부 여기(임포트 타임)서 끝내고, 런타임은 정규화된 .jtex만 읽는다.
	bool convertDDSToJTex(const std::filesystem::path& sourcePath, const std::filesystem::path& destinationPath)
	{
		std::ifstream file(sourcePath, std::ios::binary | std::ios::ate);
		if (!file)
		{
			return false;
		}

		const std::streamsize fileSize = file.tellg();
		if (fileSize < static_cast<std::streamsize>(4 + sizeof(DDSHeader)))
		{
			return false;
		}

		std::vector<uint8> bytes(static_cast<size_t>(fileSize));
		file.seekg(0);
		file.read(reinterpret_cast<char*>(bytes.data()), fileSize);
		if (!file)
		{
			return false;
		}

		const uint32 magic = *reinterpret_cast<const uint32*>(bytes.data());
		if (magic != makeFourCC('D', 'D', 'S', ' '))
		{
			return false;
		}

		const DDSHeader* header = reinterpret_cast<const DDSHeader*>(bytes.data() + 4);
		if (header->size != sizeof(DDSHeader) || header->ddspf.size != sizeof(DDSPixelFormat))
		{
			return false;
		}

		// cubemap/volume은 미지원.
		constexpr uint32 DDSCAPS2_CUBEMAP = 0x200;
		constexpr uint32 DDSCAPS2_VOLUME = 0x200000;
		if ((header->caps2 & (DDSCAPS2_CUBEMAP | DDSCAPS2_VOLUME)) != 0)
		{
			return false;
		}

		size_t dataOffset = 4 + sizeof(DDSHeader);
		uint32 format = DXGI_FORMAT_UNKNOWN;
		constexpr uint32 DDPF_FOURCC = 0x4;
		constexpr uint32 DDS_RESOURCE_DIMENSION_TEXTURE2D = 3;
		if ((header->ddspf.flags & DDPF_FOURCC) != 0)
		{
			const uint32 fourCC = header->ddspf.fourCC;
			if (fourCC == makeFourCC('D', 'X', '1', '0'))
			{
				if (static_cast<size_t>(fileSize) < dataOffset + sizeof(DDSHeaderDXT10))
				{
					return false;
				}
				const DDSHeaderDXT10* dx10 = reinterpret_cast<const DDSHeaderDXT10*>(bytes.data() + dataOffset);
				dataOffset += sizeof(DDSHeaderDXT10);
				if (dx10->resourceDimension != DDS_RESOURCE_DIMENSION_TEXTURE2D || dx10->arraySize > 1)
				{
					return false;
				}
				format = dx10->dxgiFormat;
			}
			else if (fourCC == makeFourCC('D', 'X', 'T', '1'))
			{
				format = DXGI_FORMAT_BC1_UNORM;
			}
			else if (fourCC == makeFourCC('D', 'X', 'T', '2') || fourCC == makeFourCC('D', 'X', 'T', '3'))
			{
				format = DXGI_FORMAT_BC2_UNORM;
			}
			else if (fourCC == makeFourCC('D', 'X', 'T', '4') || fourCC == makeFourCC('D', 'X', 'T', '5'))
			{
				format = DXGI_FORMAT_BC3_UNORM;
			}
			else if (fourCC == makeFourCC('A', 'T', 'I', '1') || fourCC == makeFourCC('B', 'C', '4', 'U'))
			{
				format = DXGI_FORMAT_BC4_UNORM;
			}
			else if (fourCC == makeFourCC('A', 'T', 'I', '2') || fourCC == makeFourCC('B', 'C', '5', 'U'))
			{
				format = DXGI_FORMAT_BC5_UNORM;
			}
		}
		else if (header->ddspf.rgbBitCount == 32)
		{
			if (header->ddspf.rBitMask == 0x000000ff)
			{
				format = DXGI_FORMAT_R8G8B8A8_UNORM;
			}
			else if (header->ddspf.rBitMask == 0x00ff0000)
			{
				format = DXGI_FORMAT_B8G8R8A8_UNORM;
			}
		}

		uint32 bytesPerElement = 0;
		bool blockCompressed = false;
		if (format == DXGI_FORMAT_UNKNOWN || !J::GetTextureFileFormatLayout(format, bytesPerElement, blockCompressed))
		{
			return false;
		}

		const uint32 width = header->width;
		const uint32 height = header->height;
		if (width == 0 || height == 0)
		{
			return false;
		}

		// mip 데이터가 파일에 실제로 들어있는 만큼만 굽는다.
		struct MipRange
		{
			size_t offset;
			uint32 rowBytes;
			uint32 rowCount;
		};
		std::vector<MipRange> mips;
		const uint32 headerMipCount = std::max(1u, header->mipMapCount);
		size_t offset = dataOffset;
		for (uint32 mip = 0; mip < headerMipCount; ++mip)
		{
			const uint32 mipWidth = std::max(1u, width >> mip);
			const uint32 mipHeight = std::max(1u, height >> mip);
			const uint32 rowBytes = blockCompressed ? std::max(1u, (mipWidth + 3) / 4) * bytesPerElement : mipWidth * bytesPerElement;
			const uint32 rowCount = blockCompressed ? std::max(1u, (mipHeight + 3) / 4) : mipHeight;
			const size_t mipSize = static_cast<size_t>(rowBytes) * rowCount;
			if (offset + mipSize > static_cast<size_t>(fileSize))
			{
				break;
			}

			mips.push_back({ offset, rowBytes, rowCount });
			offset += mipSize;
		}

		if (mips.empty())
		{
			return false;
		}

		std::ofstream output(destinationPath, std::ios::binary);
		if (!output)
		{
			return false;
		}

		J::JTextureFileHeader outputHeader = {};
		memcpy(outputHeader.magic, J::JTEXTURE_FILE_MAGIC, sizeof(J::JTEXTURE_FILE_MAGIC));
		outputHeader.version = J::JTEXTURE_FILE_VERSION;
		outputHeader.dxgiFormat = format;
		outputHeader.width = width;
		outputHeader.height = height;
		outputHeader.mipCount = static_cast<uint32>(mips.size());
		output.write(reinterpret_cast<const char*>(&outputHeader), sizeof(outputHeader));

		for (const MipRange& mip : mips)
		{
			J::JTextureFileMipHeader mipHeader = { mip.rowBytes, mip.rowCount };
			output.write(reinterpret_cast<const char*>(&mipHeader), sizeof(mipHeader));
			output.write(reinterpret_cast<const char*>(bytes.data() + mip.offset), static_cast<std::streamsize>(mip.rowBytes) * mip.rowCount);
		}

		return static_cast<bool>(output);
	}

	bool hasExtension(const std::filesystem::path& path, const std::string& extension)
	{
		std::string value = path.extension().string();
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		return value == extension;
	}

	std::string copyTexture(const std::filesystem::path& sourcePath, const std::filesystem::path& projectRoot)
	{
		if (sourcePath.empty() || !std::filesystem::exists(sourcePath))
		{
			return {};
		}

		const std::filesystem::path textureDir = projectRoot / "textures";
		std::filesystem::create_directories(textureDir);

		// DDS는 런타임이 직접 파싱하지 않도록 엔진 전용 .jtex로 구워서 넣는다.
		if (hasExtension(sourcePath, ".dds"))
		{
			std::filesystem::path cookedDestination = textureDir / sourcePath.filename();
			cookedDestination.replace_extension(".jtex");
			if (convertDDSToJTex(sourcePath, cookedDestination))
			{
				return "textures/" + cookedDestination.filename().string();
			}

			std::cerr << "Warning: DDS to jtex conversion failed, copying as-is: " << sourcePath.string() << "\n";
		}

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

		const std::filesystem::path baseTexture = resolveSourceTexture(fbxDirectory, importedMaterial.diffuseTexturePath);
		const std::string baseTexturePath = copyTexture(baseTexture, projectRoot);

		const std::filesystem::path normalTexture = !importedMaterial.normalTexturePath.empty()
			? resolveSourceTexture(fbxDirectory, importedMaterial.normalTexturePath)
			: findSiblingTexture(fbxDirectory, importedMaterial.diffuseTexturePath, "BaseColor", "Normal");
		const std::string normalTexturePath = copyTexture(normalTexture, projectRoot);

		// ORM 패킹 텍스처(R=Occlusion, G=Roughness, B=Metalness, Bistro/glTF 컨벤션):
		// FBX가 specular 슬롯으로 직접 가리키면 우선 사용하고, 없으면 BaseColor 시블링에서 찾는다.
		std::filesystem::path ormTexture = !importedMaterial.specularTexturePath.empty()
			? resolveSourceTexture(fbxDirectory, importedMaterial.specularTexturePath)
			: std::filesystem::path{};
		if (ormTexture.empty())
		{
			ormTexture = findSiblingTexture(fbxDirectory, importedMaterial.diffuseTexturePath, "BaseColor", "Specular");
		}
		const std::string ormTexturePath = copyTexture(ormTexture, projectRoot);
		const bool useOrm = !ormTexturePath.empty();

		const std::string roughnessTexturePath = useOrm
			? std::string{}
			: copyTexture(findSiblingTexture(fbxDirectory, importedMaterial.diffuseTexturePath, "BaseColor", "Roughness"), projectRoot);
		const std::string metallicTexturePath = useOrm
			? std::string{}
			: copyTexture(findSiblingTexture(fbxDirectory, importedMaterial.diffuseTexturePath, "BaseColor", "Metalness"), projectRoot);

		nlohmann::json materialJson;
		materialJson["name"] = importedMaterial.name.empty() ? materialID : importedMaterial.name;
		materialJson["shaderPath"] = useOrm ? "shader/gbuffer_orm.hlsl" : "shader/gbuffer_albedo.hlsl";
		materialJson["enableAlphaBlend"] = false;
		// 셰이더에서 상수는 텍스처 값에 곱해지는 multiplier이므로, 텍스처가 있으면 1.0으로 둔다.
		materialJson["constants"] =
		{
			{ "enabled", true },
			{ "baseColor", { importedMaterial.baseColor.x, importedMaterial.baseColor.y, importedMaterial.baseColor.z, importedMaterial.baseColor.w } },
			{ "roughness", (useOrm || !roughnessTexturePath.empty()) ? 1.0f : 0.5f },
			{ "metallic", (useOrm || !metallicTexturePath.empty()) ? 1.0f : 0.0f },
		};
		materialJson["textures"] = nlohmann::json::array();

		if (!baseTexturePath.empty())
		{
			materialJson["textures"].push_back({ { "name", "BaseTexture" }, { "path", baseTexturePath } });
		}
		if (!normalTexturePath.empty())
		{
			materialJson["textures"].push_back({ { "name", "NormalTexture" }, { "path", normalTexturePath } });
		}
		if (useOrm)
		{
			materialJson["textures"].push_back({ { "name", "ORMTexture" }, { "path", ormTexturePath } });
		}
		if (!roughnessTexturePath.empty())
		{
			materialJson["textures"].push_back({ { "name", "RoughnessTexture" }, { "path", roughnessTexturePath } });
		}
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
		{ "light", { { "active", true }, { "type", "point" }, { "color", { 1.0f, 0.95f, 0.85f } }, { "intensity", 150.0f }, { "range", 60.0f } } }
	});
	// 전역 디렉셔널 태양광. rotation의 +Z(forward)가 빛 방향 (pitch 50도 아래, yaw -30도).
	entitiesJson.push_back(
	{
		{ "stableID", "ent_sun" },
		{ "name", "Sun" },
		{ "active", true },
		{ "transform", { { "translation", { 0.0f, 50.0f, 0.0f } }, { "rotation", { 0.873f, -0.524f, 0.0f } }, { "scale", { 1.0f, 1.0f, 1.0f } } } },
		{ "light", { { "active", true }, { "type", "directional" }, { "color", { 1.0f, 0.96f, 0.88f } }, { "intensity", 5.0f }, { "range", 0.0f } } }
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
