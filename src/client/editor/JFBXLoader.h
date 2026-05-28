#pragma once
#ifndef __J_FBX_LOADER_H__
#define __J_FBX_LOADER_H__

#include <vector>
#include "client/OpenFBX/ofbx.h"
#include "engine/asset/JMesh.h"

class JFBXLoader
{
public:
	JFBXLoader();
	~JFBXLoader();

	struct ParsingData
	{
		std::vector<float> positions;
		std::vector<float> normals;
		std::vector<float> texcoords;
		std::vector<float> colors;
		std::vector<float> tangents;
		std::vector<float> bitangents;
		std::vector<uint32_t> indices;
		std::vector<J::Engine::JMesh::SubMeshInfo> subMeshes;

		void clear()
		{
			positions.clear();
			normals.clear();
			texcoords.clear();
			colors.clear();
			tangents.clear();
			bitangents.clear();
			indices.clear();
			subMeshes.clear();
		}
	};


	struct ImportedMaterial
	{
		std::string name;
		JVec4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		std::string diffuseTexturePath;
		std::string normalTexturePath;
		std::string specularTexturePath;
	};

	J::Engine::JMesh* LoadFBX(const char* filename);
	J::Engine::JMesh* LoadFBX(const char* filename, std::vector<ImportedMaterial>* outMaterials);

private:

	void extractMesh(const ofbx::Mesh& mesh, uint32_t materialBaseIndex, ParsingData& parsingData);
	void setMesh(J::Engine::JMesh* mesh, ParsingData& parsingData);
};

#endif
