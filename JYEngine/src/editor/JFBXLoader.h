#pragma once
#ifndef __J_FBX_LOADER_H__
#define __J_FBX_LOADER_H__

#include <vector>
#include "fbxsdk.h"
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


	J::Engine::JMesh* LoadFBX(const char* filename);

private:

	void initialize();
	void destroy();

	void extractMesh(FbxNode* node, ParsingData& parsingData);
	void setMesh(J::Engine::JMesh* mesh, ParsingData& parsingData);

	::FbxManager* _fbxManager = nullptr;
	::FbxScene* _fbxScene = nullptr;
};

#endif