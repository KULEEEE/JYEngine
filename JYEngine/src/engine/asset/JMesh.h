#pragma once
#ifndef __J_MESH_H__
#define __J_MESH_H__

#include "engine/precompile.h"

J_ENGINE_BEGIN

class JMesh
{

public:

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

	void SetPositions(vector<JVec4>&& data) { _positions = std::move(data); }
	void SetTexcoords(vector<JVec2>&& data, int index) { index ? (_texcoords0 = std::move(data)) : (_texcoords1 = std::move(data)); }
	void SetNormals(vector<JVec3>&& data) { _normals = std::move(data); }
	void SetColors(vector<JVec4>&& data) { _colors = std::move(data); }
	void SetTangents(vector<JVec3>&& data) { _tangents = std::move(data); }
	void SetBitangents(vector<JVec3>&& data) { _bitangents = std::move(data); }
	void SetBoneIndices(vector<JVec4>&& data) { _boneIndices = std::move(data); }
	void SetBoneWeights(vector<JVec4>&& data) { _boneWeights = std::move(data); }
	void SetIndices(vector<uint32>&& data) { _indices = std::move(data); }

	const vector<JVec4>& GetPositions() { return _positions; }
	const vector<JVec2>& GetTexcoords(int index) { return index ? _texcoords0 : _texcoords1; }
	const vector<JVec3>& GetNormals() { return _normals; }
	const vector<JVec4>& GetColors() { return _colors; }
	const vector<JVec3>& GetTangents() { return _tangents; }
	const vector<JVec3>& GetBitangents() { return _bitangents; }
	const vector<JVec4>& GetBoneIndices() { return _boneIndices; }
	const vector<JVec4>& GetBoneWeights() { return _boneWeights; }
	const vector<uint32>& GetIndices() { return _indices; }

	const size_t GetVertexCount() { return _positions.size(); }
private:

	vector<JVec4> _positions;
	vector<JVec2> _texcoords0;
	vector<JVec2> _texcoords1;
	vector<JVec3> _normals;
	vector<JVec4> _colors;
	vector<JVec3> _tangents;
	vector<JVec3> _bitangents;
	vector<JVec4> _boneIndices;
	vector<JVec4> _boneWeights;
	vector<uint32> _indices;

	ComPtr<ID3D12Resource> _vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW _vertexBufferView = {};
};

J_ENGINE_END
#endif