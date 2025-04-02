#include "engine/asset/JMesh.h"

J_ENGINE_BEGIN

JMesh::JMesh()
{
}

JMesh::JMesh(const JMesh& o)
	: _positions(o._positions)
	, _texcoords0(o._texcoords0)
	, _texcoords1(o._texcoords1)
	, _normals(o._normals)
	, _colors(o._colors)
	, _tangents(o._tangents)
	, _bitangents(o._bitangents)
	, _boneIndices(o._boneIndices)
	, _boneWeights(o._boneWeights)
	, _indices(o._indices)
	, _subMeshes(o._subMeshes)
{
}

JMesh::JMesh(JMesh&& o) noexcept
	: _positions(std::move(o._positions))
	, _texcoords0(std::move(o._texcoords0))
	, _texcoords1(std::move(o._texcoords1))
	, _normals(std::move(o._normals))
	, _colors(std::move(o._colors))
	, _tangents(std::move(o._tangents))
	, _bitangents(std::move(o._bitangents))
	, _boneIndices(std::move(o._boneIndices))
	, _boneWeights(std::move(o._boneWeights))
	, _indices(std::move(o._indices))
	, _subMeshes(std::move(o._subMeshes))
{
}

JMesh& JMesh::operator=(const JMesh& o)
{
	_positions = o._positions;
	_texcoords0 = o._texcoords0;
	_texcoords1 = o._texcoords1;
	_normals = o._normals;
	_colors = o._colors;
	_tangents = o._tangents;
	_bitangents = o._bitangents;
	_boneIndices = o._boneIndices;
	_boneWeights = o._boneWeights;
	_indices = o._indices;
	_subMeshes = o._subMeshes;

	return *this;
}

JMesh::~JMesh()
{
}

J_ENGINE_END