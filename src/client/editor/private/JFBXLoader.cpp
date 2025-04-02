#include "client/editor/JFBXLoader.h"

#include "engine/JHashFunction.h"

#include <map>
#include <unordered_map>
#include <utility>

using namespace std;

static double s_scaleFactor = 0.1;

static FbxAMatrix getGeometryMatrix(const FbxNode* const fbxNode)
{
	return FbxAMatrix
	(
		fbxNode->GetGeometricTranslation(FbxNode::eSourcePivot),
		fbxNode->GetGeometricRotation(FbxNode::eSourcePivot),
		fbxNode->GetGeometricScaling(FbxNode::eSourcePivot)
	);
}

static FbxAMatrix getGeometryVectorMatrix(const FbxNode* const fbxNode)
{
	return FbxAMatrix
	{
		FbxVector4{ 0.0 , 0.0 , 0.0 ,0.0 },
		fbxNode->GetGeometricRotation(FbxNode::eSourcePivot),
		fbxNode->GetGeometricScaling(FbxNode::eSourcePivot)
	}.Inverse().Transpose();
}

struct VertexIdentity
{
public:
	uint32 subMeshLocalVertexIndex = 0;
	uint32 controlPointIndex = 0;

	JVec4 position;
	JVec4 color;
	JVec2 uv0;
	JVec2 uv1;
	JVec3 normal;
	JVec4 tangent;
	JVec3 binormal;

};

template <typename T>
static T retrieveElement(
	const FbxLayerElementTemplate<T>* const layer,
	int32 controlPointIndex,
	int32 triangleBasedVertexIndex,
	int32 triangleIndex)
{
	const FbxLayerElementArrayTemplate<T>& directArray = layer->GetDirectArray();
	const FbxLayerElementArrayTemplate<int32>& indexArray = layer->GetIndexArray();

	int32 usedIndex = -1;

	switch (layer->GetMappingMode())
	{
	case FbxLayerElement::EMappingMode::eByControlPoint:
		usedIndex = controlPointIndex;
		break;

	case FbxLayerElement::EMappingMode::eByPolygonVertex:
		usedIndex = triangleBasedVertexIndex;
		break;

	case FbxLayerElement::EMappingMode::eByPolygon:
		usedIndex = triangleIndex;
		break;

	case FbxLayerElement::EMappingMode::eAllSame:
		usedIndex = 0;
		break;

	default:
		break;
	}

	switch (layer->GetReferenceMode())
	{
		// 둘이 같은 의미임. FBX v5.0 이하의 파일은 eIndex로 저장되어 있고,
		// 6.0 이상은 eIndexToDirect로 저장되어 있음.
	case FbxLayerElement::EReferenceMode::eIndex:
	case FbxLayerElement::EReferenceMode::eIndexToDirect:
		usedIndex = indexArray.GetAt(usedIndex);
		break;
	default:  break;
	}

	return directArray.GetAt(usedIndex);
}

VertexIdentity parseVertex(
	const FbxMesh* const fbxMesh,
	const FbxMatrix& geomMatrix,
	const FbxMatrix& geomVectorMatrix,
	int32 controlPointIndex,
	int32 vertexCounter,
	int32 polygonIndex,
	uint32 vertexIndexInPolygon)
{
	VertexIdentity vertex;

	vertex.controlPointIndex = static_cast<uint32>(controlPointIndex);
	
	//Position
	const FbxVector4* const pFbxPositions = fbxMesh->GetControlPoints();
	const FbxVector4& fbxPosition = geomMatrix.MultNormalize(pFbxPositions[controlPointIndex]);
	vertex.position = JVec4
	{
		static_cast<float>(fbxPosition[0] * s_scaleFactor),
		static_cast<float>(fbxPosition[1] * s_scaleFactor),
		static_cast<float>(fbxPosition[2] * s_scaleFactor),
		static_cast<float>(fbxPosition[3])
	};

	//Color
	if (nullptr != fbxMesh->GetElementVertexColor())
	{
		const FbxColor& fbxColor = retrieveElement(fbxMesh->GetElementVertexColor(), controlPointIndex, vertexCounter, polygonIndex);
		vertex.color = JVec4
		{
			static_cast<float>(fbxColor[0]),
			static_cast<float>(fbxColor[1]),
			static_cast<float>(fbxColor[2]),
			static_cast<float>(fbxColor[3])
		};
	}

	//UV0
	if (nullptr != fbxMesh->GetElementUV(0))
	{
		const FbxVector2& fbxUV0 = retrieveElement(fbxMesh->GetElementUV(0), controlPointIndex, vertexCounter, polygonIndex);
		vertex.uv0 = JVec2
		{
			static_cast<float>(fbxUV0[0]),
			static_cast<float>(fbxUV0[1])
		};
	}

	//UV1
	if(nullptr != fbxMesh->GetElementUV(1))
	{
		const FbxVector2& fbxUV1 = retrieveElement(fbxMesh->GetElementUV(1), controlPointIndex, vertexCounter, polygonIndex);
		vertex.uv1 = JVec2
		{
			static_cast<float>(fbxUV1[0]),
			static_cast<float>(fbxUV1[1])
		};
	}

	//Normal
	if (nullptr != fbxMesh->GetElementNormal())
	{
		FbxVector4 fbxNormal = retrieveElement(fbxMesh->GetElementNormal(), controlPointIndex, vertexCounter, polygonIndex);
		fbxNormal = geomVectorMatrix.MultNormalize(fbxNormal);
		vertex.normal = JVec3
		{
			static_cast<float>(fbxNormal[0]),
			static_cast<float>(fbxNormal[1]),
			static_cast<float>(fbxNormal[2])
		};
	}

	//Tangent
	if (nullptr != fbxMesh->GetElementTangent())
	{
		FbxVector4 fbxTangent = retrieveElement(fbxMesh->GetElementTangent(), controlPointIndex, vertexCounter, polygonIndex);
		fbxTangent = geomVectorMatrix.MultNormalize(fbxTangent);
		vertex.tangent = JVec4
		{
			static_cast<float>(fbxTangent[0]),
			static_cast<float>(fbxTangent[1]),
			static_cast<float>(fbxTangent[2]),
			static_cast<float>(fbxTangent[3])
		};
	}

	//Binormal
	if (nullptr != fbxMesh->GetElementBinormal())
	{
		FbxVector4 fbxBinormal = retrieveElement(fbxMesh->GetElementBinormal(), controlPointIndex, vertexCounter, polygonIndex);
		fbxBinormal = geomVectorMatrix.MultNormalize(fbxBinormal);
		vertex.binormal = JVec3
		{
			static_cast<float>(fbxBinormal[0]),
			static_cast<float>(fbxBinormal[1]),
			static_cast<float>(fbxBinormal[2])
		};
	}

	return vertex;
}

void loadVertex(
	const VertexIdentity& vertexIdentity,
	vector<uint32>& controlPointBuffer,
	vector<JVec4>& positionBuffer,
	vector<JVec4>& colorBuffer,
	vector<JVec2>& uv0Buffer,
	vector<JVec2>& uv1Buffer,
	vector<JVec3>& normalBuffer,
	vector<JVec4>& tangentBuffer,
	vector<JVec3>& binormalBuffer)
{
	positionBuffer.push_back(vertexIdentity.position);
	controlPointBuffer.push_back(vertexIdentity.controlPointIndex);

	colorBuffer.push_back(vertexIdentity.color);
	uv0Buffer.push_back(vertexIdentity.uv0);
	uv1Buffer.push_back(vertexIdentity.uv1);
	normalBuffer.push_back(vertexIdentity.normal);
	tangentBuffer.push_back(vertexIdentity.tangent);
	binormalBuffer.push_back(vertexIdentity.binormal);
}

void dumpVertex(
	JFBXLoader::ParsingData& parsingData,
	vector<JVec4>& positionsBuffer,
	vector<JVec4>& colorsBuffer,
	vector<JVec2>& uv0Buffer,
	vector<JVec2>& uv1Buffer,
	vector<JVec3>& normalsBuffer,
	vector<JVec4>& tangentsBuffer,
	vector<JVec3>& binormalsBuffer,
	vector<uint32>& indexBuffer)
{
	//Position
	for (JVec4& pos : positionsBuffer)
	{
		float* posPtr = (float*)pos;
		size_t posSize = 4;
		parsingData.positions.insert(parsingData.positions.end(), posPtr, posPtr+posSize);
	}

	//Color
	for (JVec4& col : colorsBuffer)
	{
		float* colPtr = (float*)col;
		size_t colSize = 4;
		parsingData.colors.insert(parsingData.colors.end(), colPtr, colPtr+colSize);
	}

	//UV0
	for (JVec2& tex : uv0Buffer)
	{
		float* texPtr = (float*)tex;
		size_t texSize = 2;
		parsingData.texcoords.insert(parsingData.texcoords.end(), texPtr, texPtr + texSize);
	}

	//Normal
	for (JVec3& nor : normalsBuffer)
	{
		float* norPtr = (float*)nor;
		size_t norSize = 3;
		parsingData.normals.insert(parsingData.normals.end(), norPtr, norPtr + norSize);
	}

	//Tangent
	for (JVec4& tan : tangentsBuffer)
	{
		float* tanPtr = (float*)tan;
		size_t tanSize = 4;
		parsingData.tangents.insert(parsingData.tangents.end(), tanPtr, tanPtr + tanSize);
	}

	//Binormal
	for (JVec3& bin : binormalsBuffer)
	{
		float* binPtr = (float*)bin;
		size_t binSize = 3;
		parsingData.bitangents.insert(parsingData.bitangents.end(), binPtr, binPtr + binSize);
	}

	//Index
	parsingData.indices.insert(parsingData.indices.end(), indexBuffer.begin(), indexBuffer.end());
}

JFBXLoader::JFBXLoader()
{
	initialize();
}

JFBXLoader::~JFBXLoader()
{
	_fbxScene->Destroy();
	_fbxManager->Destroy();
}

void JFBXLoader::initialize()
{
	_fbxManager = FbxManager::Create();
	
	FbxIOSettings* ios = FbxIOSettings::Create(_fbxManager, IOSROOT);
	_fbxManager->SetIOSettings(ios);

	_fbxScene = FbxScene::Create(_fbxManager, "Scene");
}

J::Engine::JMesh* JFBXLoader::LoadFBX(const char* filename)
{
	FbxImporter* importer = FbxImporter::Create(_fbxManager, "");
	if (!importer->Initialize(filename, -1, _fbxManager->GetIOSettings()))
	{
		return nullptr;
	}

	if (!importer->Import(_fbxScene))
	{
		return nullptr;
	}

	FbxNode* node = _fbxScene->GetRootNode();

	ParsingData parsingData;

	extractMesh(node, parsingData);

	J::Engine::JMesh* mesh = new J::Engine::JMesh;

	setMesh(mesh, parsingData);

	importer->Destroy();

	return mesh;
}

void JFBXLoader::extractMesh(FbxNode* node, ParsingData& parsingData)
{
	if (!node) return;

	for (int i = 0; i < node->GetChildCount(); ++i) 
	{
		extractMesh(node->GetChild(i), parsingData);
	}

	FbxMesh* fbxMesh = node->GetMesh();

	if (nullptr == fbxMesh)
	{
		return;
	}

	// 삼각형 폴리곤 메쉬가 아닌 경우 삼각화 함수를 이용하여 변경
	if (!fbxMesh->IsTriangleMesh())
	{
		FbxGeometryConverter meshConverter(_fbxManager);
		fbxMesh = static_cast<FbxMesh*>(meshConverter.Triangulate(fbxMesh, true, true));

		if(nullptr == fbxMesh)
		{
			return;
		} 
	}

	auto nodeTranslation = node->EvaluateLocalTranslation();
	nodeTranslation *= s_scaleFactor;

	fbxMesh->GenerateNormals();
	fbxMesh->GenerateTangentsDataForAllUVSets();

	const FbxMatrix geoMatrix = getGeometryMatrix(node);
	const FbxMatrix geomVectorMatrix = getGeometryVectorMatrix(node);

	map<int, int> materialIndex2SubMeshIndexMap;

	const FbxLayerElementArrayTemplate<int>* materialIndices = nullptr;
	FbxGeometryElement::EMappingMode materialMappingMode = FbxGeometryElement::eNone;
	FbxGeometryElementMaterial* const eleMaterial = fbxMesh->GetElementMaterial();

	FbxSurfaceMaterial* const fbxMaterial = node->GetMaterial(0);
	const int32 allSameMaterialIndex = 0; // TEMP

	if (eleMaterial)
	{
		materialIndices = &(eleMaterial->GetIndexArray());
		materialMappingMode = eleMaterial->GetMappingMode();

		if (FbxGeometryElement::eByPolygon == materialMappingMode)
		{
			uint32 polygonCount = fbxMesh->GetPolygonCount();

			for (uint32 polygonIdx = 0; polygonIdx < polygonCount; ++polygonCount)
			{
				const uint32 materialIndex = materialIndices->GetAt(polygonIdx);

				if (materialIndex2SubMeshIndexMap.find(materialIndex) != materialIndex2SubMeshIndexMap.end())
				{
					materialIndex2SubMeshIndexMap.insert({ materialIndex, 0 });
				}
			}
		}
		else if (FbxGeometryElement::eAllSame == materialMappingMode)
		{
			materialIndex2SubMeshIndexMap.insert({ allSameMaterialIndex, 0});
		}
	}

	vector<J::Engine::JMesh::SubMeshInfo>& subMeshes = parsingData.subMeshes;
	uint32 submeshCount = 0;

	for (auto& iter : materialIndex2SubMeshIndexMap)
	{
		J::Engine::JMesh::SubMeshInfo subMesh;
		subMesh.materialIndex = iter.first;
		iter.second++;

		subMeshes.push_back(subMesh);
	}

	if (subMeshes.empty())
	{
		// TODO : Default Material
		J::Engine::JMesh::SubMeshInfo subMesh;
		subMesh.materialIndex = 0;

		subMeshes.push_back(subMesh);
	}

	const uint32 numSubMeshes = parsingData.subMeshes.size();
	vector<vector<JVec4>> positionBuffers, colorBuffers, tangentBuffers;
	vector<vector<JVec2>> uv0Buffers, uv1Buffers;
	vector<vector<JVec3>> normalBuffers, binormalBuffers;
	vector<vector<uint32>> indexBuffers, controlPointBuffers;

	vector<unordered_map<uint32, uint32>> subMeshVertex2IndexMaps;
	vector<vector<VertexIdentity>> subMeshVertexLists;
	{
		positionBuffers.resize(numSubMeshes);
		colorBuffers.resize(numSubMeshes);
		tangentBuffers.resize(numSubMeshes);
		uv0Buffers.resize(numSubMeshes);
		uv1Buffers.resize(numSubMeshes);
		normalBuffers.resize(numSubMeshes);
		binormalBuffers.resize(numSubMeshes);
		indexBuffers.resize(numSubMeshes);
		controlPointBuffers.resize(numSubMeshes);

		subMeshVertex2IndexMaps.resize(numSubMeshes);
		subMeshVertexLists.resize(numSubMeshes);
	}

	// <key: control point index, value: [sub mesh index, sub mesh vertex count]>
	unordered_map<uint32, vector<pair<uint32, uint32>>> controlPointInfoMap;

	uint32 vertexCounter = 0;
	// triangle Count
	uint32 polygonCount = static_cast<uint32>(fbxMesh->GetPolygonCount());

	for (uint32 polygonIdx = 0; polygonIdx < polygonCount; ++polygonIdx)
	{
		uint32 polygonSize = fbxMesh->mPolygons[polygonIdx].mSize;

		int32 materialIndex = -1;

		if (materialMappingMode == FbxGeometryElement::eByPolygon)
		{
			materialIndex = materialIndices->GetAt(static_cast<int32>(polygonIdx));
		}
		else if (materialMappingMode == FbxGeometryElement::eAllSame)
		{
			// material이 하나만 적용되는 상태
			materialIndex = allSameMaterialIndex;
		}

		vector<VertexIdentity> vertices;

		// control point로부터 정점 데이터 추출
		for (int32 positionInPolygon = 0; positionInPolygon < polygonSize; ++positionInPolygon)
		{
			const uint32 controlPointIndex = fbxMesh->GetPolygonVertex(static_cast<int32>(polygonIdx), positionInPolygon);

			VertexIdentity vertex = parseVertex(
				fbxMesh,
				geoMatrix,
				geomVectorMatrix,
				controlPointIndex,
				static_cast<int32>(vertexCounter),
				static_cast<int32>(polygonIdx),
				positionInPolygon
			);

			++vertexCounter;
			vertices.push_back(vertex);
		}

		int32 subMeshIndex = materialIndex != -1 ? materialIndex2SubMeshIndexMap[materialIndex] : 0;
		unordered_map<uint32, uint32>& curSubMeshVertex2IndexMap = subMeshVertex2IndexMaps[subMeshIndex];
		vector<VertexIdentity>& curSubMeshVertexList = subMeshVertexLists[subMeshIndex];
		vector<uint32>& indexBuffer = indexBuffers[subMeshIndex];

		for (uint32 cnt = 0; cnt < vertices.size(); ++cnt)
		{
			VertexIdentity vertex = vertices[cnt];
			vertex.position.x += static_cast<float>(nodeTranslation.mData[0]);
			vertex.position.y += static_cast<float>(nodeTranslation.mData[1]);
			vertex.position.z += static_cast<float>(nodeTranslation.mData[2]);

			vector<pair<uint32, uint32>>* mappedVertexIndices = nullptr;
			auto foundIter = controlPointInfoMap.find(vertex.controlPointIndex);
			if (foundIter == controlPointInfoMap.end())
			{
				vector<pair<uint32, uint32>> init = { { 0,0 } };
				controlPointInfoMap.insert({ vertex.controlPointIndex, init });
				mappedVertexIndices = &controlPointInfoMap[vertex.controlPointIndex];
			}
			else
			{
				mappedVertexIndices = &foundIter->second;
			}

			// 정점 중복 체크
			auto r = curSubMeshVertex2IndexMap.find(JHashFunction::TypeCrc32(vertex));
			uint32 index = 0;

			if (r != curSubMeshVertex2IndexMap.end())
			{
				index = r->second;
			}
			else
			{
				index = static_cast<uint32>(curSubMeshVertexList.size());
				curSubMeshVertex2IndexMap.insert({ JHashFunction::TypeCrc32(vertex), index });

				curSubMeshVertexList.push_back(vertex);

				loadVertex(
					vertex,
					controlPointBuffers[subMeshIndex],
					positionBuffers[subMeshIndex],
					colorBuffers[subMeshIndex],
					uv0Buffers[subMeshIndex],
					uv1Buffers[subMeshIndex],
					normalBuffers[subMeshIndex],
					tangentBuffers[subMeshIndex],
					binormalBuffers[subMeshIndex]);

				mappedVertexIndices->push_back({ subMeshIndex, index });
			}
			indexBuffer.push_back(index);
		}
	}

	for (uint32 subMeshIndex = 0; subMeshIndex < numSubMeshes; ++subMeshIndex)
	{
		vector<uint32>& indexBuffer = indexBuffers[subMeshIndex];

		uint32 indexOffset = 0U;

		for (uint32 prevSubMeshIter = 0; prevSubMeshIter < subMeshIndex; ++prevSubMeshIter)
		{
			/*
				position은 vertex buffer 구성에 반드시 참여하는 attribute이므로
				#positions == #vertices 이다.
			*/
			indexOffset += static_cast<uint32>(positionBuffers[prevSubMeshIter].size());
		}

		for (uint32& index : indexBuffer)
		{
			index += indexOffset;
		}
	}
	
	for(uint32 subMeshIndex = 0; subMeshIndex < numSubMeshes; ++subMeshIndex)
	{
		dumpVertex(
			parsingData,
			positionBuffers[subMeshIndex],
			colorBuffers[subMeshIndex],
			uv0Buffers[subMeshIndex],
			uv1Buffers[subMeshIndex],
			normalBuffers[subMeshIndex],
			tangentBuffers[subMeshIndex],
			binormalBuffers[subMeshIndex],
			indexBuffers[subMeshIndex]);

		J::Engine::JMesh::SubMeshInfo subMesh;
		subMesh.endIndex = parsingData.indices.size();
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


