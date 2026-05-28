#include "client/editor/JFBXLoader.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>

using namespace std;

static const float s_scaleFactor = 0.1f;

JFBXLoader::JFBXLoader() {}
JFBXLoader::~JFBXLoader() {}

J::Engine::JMesh* JFBXLoader::LoadFBX(const char* filename)
{
	return LoadFBX(filename, nullptr);
}

namespace
{
	std::string toString(const ofbx::DataView& value)
	{
		if (value.begin == nullptr || value.end == nullptr || value.end <= value.begin)
		{
			return {};
		}

		return std::string(reinterpret_cast<const char*>(value.begin), reinterpret_cast<const char*>(value.end));
	}

	std::string texturePath(const ofbx::Texture* texture)
	{
		if (texture == nullptr)
		{
			return {};
		}

		std::string path = toString(texture->getRelativeFileName());
		if (path.empty())
		{
			path = toString(texture->getFileName());
		}
		return path;
	}

	JFBXLoader::ImportedMaterial importMaterial(const ofbx::Material& material)
	{
		JFBXLoader::ImportedMaterial imported;
		imported.name = material.name;
		const ofbx::Color diffuse = material.getDiffuseColor();
		const float diffuseFactor = static_cast<float>(material.getDiffuseFactor());
		imported.baseColor = {
			diffuse.r * diffuseFactor,
			diffuse.g * diffuseFactor,
			diffuse.b * diffuseFactor,
			1.0f
		};
		imported.diffuseTexturePath = texturePath(material.getTexture(ofbx::Texture::DIFFUSE));
		imported.normalTexturePath = texturePath(material.getTexture(ofbx::Texture::NORMAL));
		imported.specularTexturePath = texturePath(material.getTexture(ofbx::Texture::SPECULAR));
		return imported;
	}

	JFBXLoader::ImportedNode importNode(const ofbx::Mesh& mesh)
	{
		JFBXLoader::ImportedNode node;
		node.name = mesh.name;
		return node;
	}

	ofbx::DMatrix multiplyMatrix(const ofbx::DMatrix& lhs, const ofbx::DMatrix& rhs)
	{
		ofbx::DMatrix result = {};
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				double value = 0.0;
				for (int k = 0; k < 4; ++k)
				{
					value += lhs.m[row + k * 4] * rhs.m[k + column * 4];
				}
				result.m[row + column * 4] = value;
			}
		}
		return result;
	}

	ofbx::DMatrix identityMatrix()
	{
		return ofbx::DMatrix{ 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0 };
	}

	ofbx::DVec3 transformPoint(const ofbx::DMatrix& matrix, const ofbx::Vec3& point)
	{
		return {
			matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
			matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
			matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]
		};
	}

	ofbx::DVec3 toDVec3(const ofbx::Vec3& vector)
	{
		return { vector.x, vector.y, vector.z };
	}

	ofbx::DVec3 transformVector(const ofbx::DMatrix& matrix, const ofbx::Vec3& vector)
	{
		return {
			matrix.m[0] * vector.x + matrix.m[4] * vector.y + matrix.m[8] * vector.z,
			matrix.m[1] * vector.x + matrix.m[5] * vector.y + matrix.m[9] * vector.z,
			matrix.m[2] * vector.x + matrix.m[6] * vector.y + matrix.m[10] * vector.z
		};
	}

	ofbx::DVec3 normalizeVector(const ofbx::DVec3& vector)
	{
		const double length = sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
		if (length <= 0.000001)
		{
			return { 0.0, 1.0, 0.0 };
		}

		return { vector.x / length, vector.y / length, vector.z / length };
	}
}

J::Engine::JMesh* JFBXLoader::LoadFBX(const char* filename, std::vector<ImportedMaterial>* outMaterials)
{
	printf("LoadFBX: %s\n", filename);
	FILE* fp = nullptr;
	fopen_s(&fp, filename, "rb");
	if (!fp)
	{
		printf("LoadFBX failed to open file: %s\n", filename);
		return nullptr;
	}

	fseek(fp, 0, SEEK_END);
	long fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	vector<ofbx::u8> buffer(fileSize);
	fread(buffer.data(), 1, fileSize, fp);
	fclose(fp);

	ofbx::LoadFlags flags =
		ofbx::LoadFlags::IGNORE_BLEND_SHAPES |
		ofbx::LoadFlags::IGNORE_CAMERAS |
		ofbx::LoadFlags::IGNORE_LIGHTS |
		ofbx::LoadFlags::IGNORE_SKIN |
		ofbx::LoadFlags::IGNORE_BONES |
		ofbx::LoadFlags::IGNORE_PIVOTS |
		ofbx::LoadFlags::IGNORE_ANIMATIONS |
		ofbx::LoadFlags::IGNORE_POSES |
		ofbx::LoadFlags::IGNORE_VIDEOS |
		ofbx::LoadFlags::IGNORE_LIMBS;

	ofbx::IScene* scene = ofbx::load(buffer.data(), static_cast<int>(fileSize), static_cast<ofbx::u16>(flags));
	if (!scene)
	{
		printf("OpenFBX load failed: %s\n", ofbx::getError());
		return nullptr;
	}

	ParsingData parsingData;
	if (outMaterials != nullptr)
	{
		outMaterials->clear();
	}

	const int meshCount = scene->getMeshCount();
	for (int m = 0; m < meshCount; ++m)
	{
		const ofbx::Mesh* mesh = scene->getMesh(m);
		if (!mesh) continue;

		const uint32_t materialBaseIndex = outMaterials != nullptr ? static_cast<uint32_t>(outMaterials->size()) : 0;
		if (outMaterials != nullptr)
		{
			const int materialCount = mesh->getMaterialCount();
			for (int materialIndex = 0; materialIndex < materialCount; ++materialIndex)
			{
				const ofbx::Material* material = mesh->getMaterial(materialIndex);
				outMaterials->push_back(material != nullptr ? importMaterial(*material) : ImportedMaterial{});
			}
		}

		extractMesh(*mesh, materialBaseIndex, parsingData);
	}

	J::Engine::JMesh* outMesh = new J::Engine::JMesh;
	setMesh(outMesh, parsingData);

	scene->destroy();

	return outMesh;
}

bool JFBXLoader::LoadFBXScene(const char* filename, ImportedScene& outScene)
{
	outScene = {};

	printf("LoadFBXScene: %s\n", filename);
	FILE* fp = nullptr;
	fopen_s(&fp, filename, "rb");
	if (!fp)
	{
		printf("LoadFBXScene failed to open file: %s\n", filename);
		return false;
	}

	fseek(fp, 0, SEEK_END);
	long fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	vector<ofbx::u8> buffer(fileSize);
	fread(buffer.data(), 1, fileSize, fp);
	fclose(fp);

	ofbx::LoadFlags flags =
		ofbx::LoadFlags::IGNORE_BLEND_SHAPES |
		ofbx::LoadFlags::IGNORE_CAMERAS |
		ofbx::LoadFlags::IGNORE_LIGHTS |
		ofbx::LoadFlags::IGNORE_SKIN |
		ofbx::LoadFlags::IGNORE_BONES |
		ofbx::LoadFlags::IGNORE_PIVOTS |
		ofbx::LoadFlags::IGNORE_ANIMATIONS |
		ofbx::LoadFlags::IGNORE_POSES |
		ofbx::LoadFlags::IGNORE_VIDEOS |
		ofbx::LoadFlags::IGNORE_LIMBS;

	ofbx::IScene* scene = ofbx::load(buffer.data(), static_cast<int>(fileSize), static_cast<ofbx::u16>(flags));
	if (!scene)
	{
		printf("OpenFBX scene load failed: %s\n", ofbx::getError());
		return false;
	}

	const int meshCount = scene->getMeshCount();
	for (int m = 0; m < meshCount; ++m)
	{
		const ofbx::Mesh* fbxMesh = scene->getMesh(m);
		if (fbxMesh == nullptr)
		{
			continue;
		}

		const uint32_t materialBaseIndex = static_cast<uint32_t>(outScene.materials.size());
		const int materialCount = fbxMesh->getMaterialCount();
		for (int materialIndex = 0; materialIndex < materialCount; ++materialIndex)
		{
			const ofbx::Material* material = fbxMesh->getMaterial(materialIndex);
			outScene.materials.push_back(material != nullptr ? importMaterial(*material) : ImportedMaterial{});
		}

		ParsingData parsingData;
		extractMesh(*fbxMesh, materialBaseIndex, parsingData, true);
		if (parsingData.positions.empty() || parsingData.indices.empty())
		{
			continue;
		}

		ImportedNode node = importNode(*fbxMesh);
		node.mesh = new J::Engine::JMesh;
		setMesh(node.mesh, parsingData);
		outScene.nodes.push_back(node);
	}

	scene->destroy();
	return !outScene.nodes.empty();
}

void JFBXLoader::extractMesh(const ofbx::Mesh& mesh, uint32_t materialBaseIndex, ParsingData& parsingData, bool bakeNodeTransform)
{
	const ofbx::GeometryData& geom = mesh.getGeometryData();
	const ofbx::DMatrix nodeMatrix = bakeNodeTransform
		? multiplyMatrix(mesh.getGlobalTransform(), mesh.getGeometricMatrix())
		: identityMatrix();

	const ofbx::Vec3Attributes positions = geom.getPositions();
	const ofbx::Vec3Attributes normals = geom.getNormals();
	const ofbx::Vec2Attributes uvs = geom.getUVs(0);
	const ofbx::Vec4Attributes colors = geom.getColors();
	const ofbx::Vec3Attributes tangents = geom.getTangents();

	const bool hasNormals = normals.values != nullptr && normals.count > 0;
	const bool hasUVs = uvs.values != nullptr && uvs.count > 0;
	const bool hasColors = colors.values != nullptr && colors.count > 0;
	const bool hasTangents = tangents.values != nullptr && tangents.count > 0;

	const int partitionCount = geom.getPartitionCount();
	for (int p = 0; p < partitionCount; ++p)
	{
		const ofbx::GeometryPartition partition = geom.getPartition(p);

		J::Engine::JMesh::SubMeshInfo subMesh;
		subMesh.materialIndex = materialBaseIndex + static_cast<uint32_t>(p);
		subMesh.startIndex = static_cast<uint32_t>(parsingData.indices.size());

		vector<int> triIndices(partition.max_polygon_triangles * 3);

		for (int pi = 0; pi < partition.polygon_count; ++pi)
		{
			const ofbx::GeometryPartition::Polygon& polygon = partition.polygons[pi];
			const uint32_t triCount = ofbx::triangulate(geom, polygon, triIndices.data());

			// FBX(right-handed, Y-up)  DirectX(left-handed, Y-up)  .
			// z   +  winding        .
			for (uint32_t t = 0; t + 2 < triCount; t += 3)
			{
				const int triangle[3] = { triIndices[t], triIndices[t + 2], triIndices[t + 1] };
				for (int k = 0; k < 3; ++k)
				{
					const int vi = triangle[k];

					const ofbx::Vec3 rawPos = positions.get(vi);
					const ofbx::DVec3 pos = bakeNodeTransform ? transformPoint(nodeMatrix, rawPos) : toDVec3(rawPos);
					parsingData.positions.push_back(static_cast<float>(pos.x) * s_scaleFactor);
					parsingData.positions.push_back(static_cast<float>(pos.y) * s_scaleFactor);
					parsingData.positions.push_back(static_cast<float>(-pos.z) * s_scaleFactor);
					parsingData.positions.push_back(1.0f);

					if (hasNormals)
					{
						const ofbx::Vec3 rawNormal = normals.get(vi);
						const ofbx::DVec3 n = normalizeVector(bakeNodeTransform ? transformVector(nodeMatrix, rawNormal) : toDVec3(rawNormal));
						parsingData.normals.push_back(static_cast<float>(n.x));
						parsingData.normals.push_back(static_cast<float>(n.y));
						parsingData.normals.push_back(static_cast<float>(-n.z));
					}

					if (hasUVs)
					{
						const ofbx::Vec2 uv = uvs.get(vi);
						parsingData.texcoords.push_back(static_cast<float>(uv.x));
						parsingData.texcoords.push_back(1.0f - static_cast<float>(uv.y));
					}

					if (hasColors)
					{
						const ofbx::Vec4 c = colors.get(vi);
						parsingData.colors.push_back(static_cast<float>(c.x));
						parsingData.colors.push_back(static_cast<float>(c.y));
						parsingData.colors.push_back(static_cast<float>(c.z));
						parsingData.colors.push_back(static_cast<float>(c.w));
					}

					if (hasTangents)
					{
						const ofbx::Vec3 rawTangent = tangents.get(vi);
						const ofbx::DVec3 tan = normalizeVector(bakeNodeTransform ? transformVector(nodeMatrix, rawTangent) : toDVec3(rawTangent));
						parsingData.tangents.push_back(static_cast<float>(tan.x));
						parsingData.tangents.push_back(static_cast<float>(tan.y));
						parsingData.tangents.push_back(static_cast<float>(-tan.z));
						parsingData.tangents.push_back(0.0f);
					}

					const uint32_t localIndex = static_cast<uint32_t>(parsingData.positions.size() / 4) - 1;
					parsingData.indices.push_back(localIndex);
				}
			}
		}

		subMesh.endIndex = static_cast<uint32_t>(parsingData.indices.size());
		parsingData.subMeshes.push_back(subMesh);
	}
}

void JFBXLoader::setMesh(J::Engine::JMesh* mesh, ParsingData& parsingData)
{
	mesh->SetPositions(std::move(parsingData.positions));
	mesh->SetNormals(std::move(parsingData.normals));
	mesh->SetTexcoords(std::move(parsingData.texcoords), 0);
	mesh->SetColors(std::move(parsingData.colors));
	mesh->SetTangents(std::move(parsingData.tangents));
	mesh->SetBitangents(std::move(parsingData.bitangents));
	mesh->SetIndices(std::move(parsingData.indices));
	mesh->SetSubMeshes(std::move(parsingData.subMeshes));
}
