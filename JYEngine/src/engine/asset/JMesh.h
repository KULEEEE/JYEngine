#pragma once
#ifndef __J_MESH_H__
#define __J_MESH_H__

#include "engine/precompile.h"

J_ENGINE_BEGIN

class JMesh
{

public:

	struct SubMeshInfo
	{
		uint32 materialIndex;
		uint32 startIndex;
		uint32 endIndex;
	};

	enum class AttributeIndex : uint8
	{
		POSITION = 0,
		TEXCOORD0 = 1,
		TEXCOORD1 = 2,
		NORMAL = 3,
		COLOR = 4,
		TANGENT = 5,
		BITANGENT = 6,
		BONE_INDEX = 7,
		BONE_WEIGHT = 8,
		INDEX = 9,
		BOUND = 10,

		TEXCOORD2 = 11,
		TEXCOORD3 = 12,
		TEXCOORD4 = 13,
		TEXCOORD5 = 14,
		TEXCOORD6 = 15,
		TEXCOORD7 = 16,
		TEXCOORD8 = 17,
	};

	JMesh();
	JMesh(const JMesh& o);
	JMesh(JMesh&& o) noexcept;
	JMesh& operator=(const JMesh& o);
	~JMesh();

	void SetPositions(vector<float>&& data) { _positions = std::move(data); }
	void SetTexcoords(vector<float>&& data, int index) { (!index) ? (_texcoords0 = std::move(data)) : (_texcoords1 = std::move(data)); }
	void SetNormals(vector<float>&& data) { _normals = std::move(data); }
	void SetColors(vector<float>&& data) { _colors = std::move(data); }
	void SetTangents(vector<float>&& data) { _tangents = std::move(data); }
	void SetBitangents(vector<float>&& data) { _bitangents = std::move(data); }
	void SetBoneIndices(vector<float>&& data) { _boneIndices = std::move(data); }
	void SetBoneWeights(vector<float>&& data) { _boneWeights = std::move(data); }
	void SetIndices(vector<uint32>&& data) { _indices = std::move(data); }
	void SetSubMeshes(vector<SubMeshInfo>&& data) { _subMeshes = std::move(data); }

	const vector<float>& GetPositions() { return _positions; }
	const vector<float>& GetTexcoords(int index) { return (!index) ? _texcoords0 : _texcoords1; }
	const vector<float>& GetNormals() { return _normals; }
	const vector<float>& GetColors() { return _colors; }
	const vector<float>& GetTangents() { return _tangents; }
	const vector<float>& GetBitangents() { return _bitangents; }
	const vector<float>& GetBoneIndices() { return _boneIndices; }
	const vector<float>& GetBoneWeights() { return _boneWeights; }
	const vector<uint32>& GetIndices() { return _indices; }
	const vector<SubMeshInfo>& GetSubMeshInfos() { return _subMeshes; }

	const size_t GetVertexCount() { return _positions.size() / 4; }
private:

	vector<float> _positions;
	vector<float> _texcoords0;
	vector<float> _texcoords1;
	vector<float> _normals;
	vector<float> _colors;
	vector<float> _tangents;
	vector<float> _bitangents;
	vector<float> _boneIndices;
	vector<float> _boneWeights;
	vector<uint32> _indices;

	vector<SubMeshInfo> _subMeshes;
};

J_ENGINE_END
#endif