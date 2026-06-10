#pragma once
#ifndef __J_MESH_H__
#define __J_MESH_H__

#include "engine/precompile.h"

#include <cfloat>

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

	struct Bounds
	{
		JVec3 min = {};
		JVec3 max = {};
		JVec3 center = {};
		JVec3 extents = {};
		bool valid = false;
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

	void SetPositions(vector<float>&& data)
	{
		_positions = std::move(data);
		updateBounds();
	}
	void SetTexcoords(vector<float>&& data, int index) { (!index) ? (_texcoords0 = std::move(data)) : (_texcoords1 = std::move(data)); }
	void SetNormals(vector<float>&& data) { _normals = std::move(data); }
	void SetColors(vector<float>&& data) { _colors = std::move(data); }
	void SetTangents(vector<float>&& data) { _tangents = std::move(data); }
	void SetBitangents(vector<float>&& data) { _bitangents = std::move(data); }
	void SetBoneIndices(vector<float>&& data) { _boneIndices = std::move(data); }
	void SetBoneWeights(vector<float>&& data) { _boneWeights = std::move(data); }
	void SetIndices(vector<uint32>&& data) { _indices = std::move(data); }
	void SetSubMeshes(vector<SubMeshInfo>&& data) { _subMeshes = std::move(data); }

	const vector<float>& GetPositions() const { return _positions; }
	const vector<float>& GetTexcoords(int index) const { return (!index) ? _texcoords0 : _texcoords1; }
	const vector<float>& GetNormals() const { return _normals; }
	const vector<float>& GetColors() const { return _colors; }
	const vector<float>& GetTangents() const { return _tangents; }
	const vector<float>& GetBitangents() const { return _bitangents; }
	const vector<float>& GetBoneIndices() const { return _boneIndices; }
	const vector<float>& GetBoneWeights() const { return _boneWeights; }
	const vector<uint32>& GetIndices() const { return _indices; }
	const vector<SubMeshInfo>& GetSubMeshInfos() const { return _subMeshes; }
	const Bounds& GetBounds() const { return _bounds; }

	const size_t GetVertexCount() const { return _positions.size() / 4; }
private:
	void updateBounds()
	{
		_bounds = {};
		if (_positions.size() < 4)
		{
			return;
		}

		JVec3 minPoint(FLT_MAX, FLT_MAX, FLT_MAX);
		JVec3 maxPoint(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (size_t i = 0; i + 3 < _positions.size(); i += 4)
		{
			const float x = _positions[i + 0];
			const float y = _positions[i + 1];
			const float z = _positions[i + 2];

			minPoint.x = std::min(minPoint.x, x);
			minPoint.y = std::min(minPoint.y, y);
			minPoint.z = std::min(minPoint.z, z);
			maxPoint.x = std::max(maxPoint.x, x);
			maxPoint.y = std::max(maxPoint.y, y);
			maxPoint.z = std::max(maxPoint.z, z);
		}

		_bounds.min = minPoint;
		_bounds.max = maxPoint;
		_bounds.center = JVec3(
			(minPoint.x + maxPoint.x) * 0.5f,
			(minPoint.y + maxPoint.y) * 0.5f,
			(minPoint.z + maxPoint.z) * 0.5f);
		_bounds.extents = JVec3(
			(maxPoint.x - minPoint.x) * 0.5f,
			(maxPoint.y - minPoint.y) * 0.5f,
			(maxPoint.z - minPoint.z) * 0.5f);
		_bounds.valid = true;
	}

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
	Bounds _bounds;
};

J_ENGINE_END
#endif
