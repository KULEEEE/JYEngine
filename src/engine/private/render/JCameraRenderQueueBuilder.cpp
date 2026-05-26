#include "engine/render/JCameraRenderQueueBuilder.h"

#include "engine/core/JJobSystem.h"
#include "engine/render/JRenderDB.h"
#include "engine/scene/JScene.h"

J_ENGINE_BEGIN

namespace
{
	XMVECTOR makePlane(float a, float b, float c, float d)
	{
		return XMPlaneNormalize(XMVectorSet(a, b, c, d));
	}

	JVec3 minVec3(const JVec3& a, const JVec3& b)
	{
		return JVec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
	}

	JVec3 maxVec3(const JVec3& a, const JVec3& b)
	{
		return JVec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
	}

	XMMATRIX makeWorldMatrix(const JScene::TransformData& transform)
	{
		const XMMATRIX scale = XMMatrixScaling(transform.scale.x, transform.scale.y, transform.scale.z);
		const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(transform.rotation.x, transform.rotation.y, transform.rotation.z);
		const XMMATRIX translation = XMMatrixTranslation(transform.translation.x, transform.translation.y, transform.translation.z);
		return scale * rotation * translation;
	}
}

void JCameraRenderQueueBuilder::Build(const Input& input, JFrameSnapshot& snapshot)
{
	if (input.drawItemCache == nullptr)
	{
		return;
	}

	for (JCameraSnapshot& cameraSnapshot : snapshot.cameras)
	{
		cameraSnapshot.opaqueDrawItemIndices.clear();
		cameraSnapshot.transparentDrawItemIndices.clear();
		cameraSnapshot.cullingTestedDrawItemCount = 0;
		cameraSnapshot.culledDrawItemCount = 0;
		cameraSnapshot.frustum = buildFrustum(cameraSnapshot.viewProjection);

		if (input.jobSystem != nullptr && input.jobSystem->IsInitialized())
		{
			buildParallel(input, cameraSnapshot);
		}
		else
		{
			buildSerial(input, cameraSnapshot);
		}
	}
}

JFrustum JCameraRenderQueueBuilder::buildFrustum(const XMMATRIX& viewProjection)
{
	XMFLOAT4X4 matrix{};
	XMStoreFloat4x4(&matrix, viewProjection);

	JFrustum frustum{};
	frustum.planes[0] = makePlane(matrix._14 + matrix._11, matrix._24 + matrix._21, matrix._34 + matrix._31, matrix._44 + matrix._41);
	frustum.planes[1] = makePlane(matrix._14 - matrix._11, matrix._24 - matrix._21, matrix._34 - matrix._31, matrix._44 - matrix._41);
	frustum.planes[2] = makePlane(matrix._14 + matrix._12, matrix._24 + matrix._22, matrix._34 + matrix._32, matrix._44 + matrix._42);
	frustum.planes[3] = makePlane(matrix._14 - matrix._12, matrix._24 - matrix._22, matrix._34 - matrix._32, matrix._44 - matrix._42);
	frustum.planes[4] = makePlane(matrix._13, matrix._23, matrix._33, matrix._43);
	frustum.planes[5] = makePlane(matrix._14 - matrix._13, matrix._24 - matrix._23, matrix._34 - matrix._33, matrix._44 - matrix._43);
	return frustum;
}

bool JCameraRenderQueueBuilder::isVisible(const JCameraSnapshot& cameraSnapshot, const JDrawItem& drawItem, const Input& input)
{
	if (drawItem.mesh == nullptr)
	{
		return false;
	}

	const JMesh::Bounds& bounds = drawItem.mesh->GetBounds();
	if (!bounds.valid)
	{
		return true;
	}

	if (input.renderDB != nullptr)
	{
		const JTransformResource* transformResource = input.renderDB->FindTransformResource(drawItem.transform);
		if (transformResource != nullptr)
		{
			return isAABBVisible(cameraSnapshot.frustum, bounds, transformResource->world);
		}
	}

	if (input.scene != nullptr)
	{
		return isAABBVisible(cameraSnapshot.frustum, bounds, makeWorldMatrix(input.scene->GetTransform(drawItem.transform)));
	}

	return true;
}

bool JCameraRenderQueueBuilder::isAABBVisible(const JFrustum& frustum, const JMesh::Bounds& bounds, const XMMATRIX& world)
{
	const JVec3 localCorners[8] =
	{
		JVec3(bounds.min.x, bounds.min.y, bounds.min.z),
		JVec3(bounds.max.x, bounds.min.y, bounds.min.z),
		JVec3(bounds.min.x, bounds.max.y, bounds.min.z),
		JVec3(bounds.max.x, bounds.max.y, bounds.min.z),
		JVec3(bounds.min.x, bounds.min.y, bounds.max.z),
		JVec3(bounds.max.x, bounds.min.y, bounds.max.z),
		JVec3(bounds.min.x, bounds.max.y, bounds.max.z),
		JVec3(bounds.max.x, bounds.max.y, bounds.max.z),
	};

	JVec3 worldMin(FLT_MAX, FLT_MAX, FLT_MAX);
	JVec3 worldMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	for (const JVec3& corner : localCorners)
	{
		const XMVECTOR worldCorner = XMVector3TransformCoord(XMVectorSet(corner.x, corner.y, corner.z, 1.0f), world);
		XMFLOAT3 stored{};
		XMStoreFloat3(&stored, worldCorner);
		const JVec3 point(stored.x, stored.y, stored.z);
		worldMin = minVec3(worldMin, point);
		worldMax = maxVec3(worldMax, point);
	}

	for (const XMVECTOR& plane : frustum.planes)
	{
		XMFLOAT4 storedPlane{};
		XMStoreFloat4(&storedPlane, plane);
		const JVec3 positiveVertex(
			storedPlane.x >= 0.0f ? worldMax.x : worldMin.x,
			storedPlane.y >= 0.0f ? worldMax.y : worldMin.y,
			storedPlane.z >= 0.0f ? worldMax.z : worldMin.z);
		const float distance =
			storedPlane.x * positiveVertex.x +
			storedPlane.y * positiveVertex.y +
			storedPlane.z * positiveVertex.z +
			storedPlane.w;
		if (distance < 0.0f)
		{
			return false;
		}
	}

	return true;
}

void JCameraRenderQueueBuilder::buildSerial(const Input& input, JCameraSnapshot& cameraSnapshot)
{
	const JDrawItemCache& drawItemCache = *input.drawItemCache;
	for (uint32 entityIndex : drawItemCache.activeDrawEntityIndices)
	{
		if (entityIndex >= drawItemCache.drawRangeByEntityIndex.size())
		{
			continue;
		}

		const JDrawRange& range = drawItemCache.drawRangeByEntityIndex[entityIndex];
		if (!range.valid)
		{
			continue;
		}

		for (uint32 i = 0; i < range.count; ++i)
		{
			const uint32 drawItemIndex = range.start + i;
			if (drawItemIndex >= drawItemCache.drawItems.size())
			{
				continue;
			}

			const JDrawItem& drawItem = drawItemCache.drawItems[drawItemIndex];
			++cameraSnapshot.cullingTestedDrawItemCount;
			if (!isVisible(cameraSnapshot, drawItem, input))
			{
				++cameraSnapshot.culledDrawItemCount;
				continue;
			}

			if (drawItem.transparent)
			{
				cameraSnapshot.transparentDrawItemIndices.push_back(drawItemIndex);
			}
			else
			{
				cameraSnapshot.opaqueDrawItemIndices.push_back(drawItemIndex);
			}
		}
	}
}

void JCameraRenderQueueBuilder::buildParallel(const Input& input, JCameraSnapshot& cameraSnapshot)
{
	const JDrawItemCache& drawItemCache = *input.drawItemCache;
	JJobSystem& jobSystem = *input.jobSystem;
	const uint32 activeCount = static_cast<uint32>(drawItemCache.activeDrawEntityIndices.size());
	constexpr uint32 MIN_CHUNK_SIZE = 256;
	if (activeCount <= MIN_CHUNK_SIZE)
	{
		buildSerial(input, cameraSnapshot);
		return;
	}

	struct ChunkResult
	{
		std::vector<uint32> opaque;
		std::vector<uint32> transparent;
		uint32 testedCount = 0;
		uint32 culledCount = 0;
	};

	const uint32 workerCount = std::max<uint32>(1, jobSystem.GetWorkerCount());
	const uint32 targetChunkCount = std::max<uint32>(1, workerCount * 2);
	const uint32 chunkSize = std::max<uint32>(MIN_CHUNK_SIZE, (activeCount + targetChunkCount - 1) / targetChunkCount);
	const uint32 chunkCount = (activeCount + chunkSize - 1) / chunkSize;
	std::vector<ChunkResult> chunkResults(chunkCount);

	jobSystem.ParallelFor(activeCount, chunkSize,
		[&drawItemCache, &chunkResults, &cameraSnapshot, &input, chunkSize](uint32 begin, uint32 end)
		{
			const uint32 chunkIndex = begin / chunkSize;
			ChunkResult& result = chunkResults[chunkIndex];

			for (uint32 activeIndex = begin; activeIndex < end; ++activeIndex)
			{
				const uint32 entityIndex = drawItemCache.activeDrawEntityIndices[activeIndex];
				if (entityIndex >= drawItemCache.drawRangeByEntityIndex.size())
				{
					continue;
				}

				const JDrawRange& range = drawItemCache.drawRangeByEntityIndex[entityIndex];
				if (!range.valid)
				{
					continue;
				}

				for (uint32 i = 0; i < range.count; ++i)
				{
					const uint32 drawItemIndex = range.start + i;
					if (drawItemIndex >= drawItemCache.drawItems.size())
					{
						continue;
					}

					const JDrawItem& drawItem = drawItemCache.drawItems[drawItemIndex];
					++result.testedCount;
					if (!isVisible(cameraSnapshot, drawItem, input))
					{
						++result.culledCount;
						continue;
					}

					if (drawItem.transparent)
					{
						result.transparent.push_back(drawItemIndex);
					}
					else
					{
						result.opaque.push_back(drawItemIndex);
					}
				}
			}
		});

	uint32 opaqueCount = 0;
	uint32 transparentCount = 0;
	for (const ChunkResult& result : chunkResults)
	{
		opaqueCount += static_cast<uint32>(result.opaque.size());
		transparentCount += static_cast<uint32>(result.transparent.size());
		cameraSnapshot.cullingTestedDrawItemCount += result.testedCount;
		cameraSnapshot.culledDrawItemCount += result.culledCount;
	}

	cameraSnapshot.opaqueDrawItemIndices.reserve(opaqueCount);
	cameraSnapshot.transparentDrawItemIndices.reserve(transparentCount);
	for (const ChunkResult& result : chunkResults)
	{
		cameraSnapshot.opaqueDrawItemIndices.insert(cameraSnapshot.opaqueDrawItemIndices.end(), result.opaque.begin(), result.opaque.end());
		cameraSnapshot.transparentDrawItemIndices.insert(cameraSnapshot.transparentDrawItemIndices.end(), result.transparent.begin(), result.transparent.end());
	}
}

J_ENGINE_END
