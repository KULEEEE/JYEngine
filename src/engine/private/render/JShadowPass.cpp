#include "engine/render/JShadowPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JDrawItemCache.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JRenderDB.h"
#include "engine/render/JRenderResource.h"
#include "engine/render/JShadowMap.h"
#include "engine/asset/JShader.h"
#include "engine/core/JHashFunction.h"

#include <algorithm>
#include <cmath>

J_ENGINE_BEGIN

namespace
{
	// 카메라 프러스텀을 거리 기준으로 나눌 누적 비율. 가까운 cascade일수록 촘촘하게.
	constexpr float CASCADE_SPLIT_RATIOS[JShadowMap::CASCADE_COUNT] = { 0.05f, 0.16f, 0.42f, 1.0f };

	struct FrustumCorners
	{
		XMVECTOR nearCorners[4];
		XMVECTOR farCorners[4];
	};

	// reverse-Z: NDC depth 1 = near plane, 0 = far plane
	FrustumCorners unprojectFrustum(const XMMATRIX& inverseViewProjection)
	{
		FrustumCorners corners{};
		const float ndcXY[4][2] = { { -1.0f, -1.0f }, { -1.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, -1.0f } };
		for (uint32 i = 0; i < 4; ++i)
		{
			corners.nearCorners[i] = XMVector3TransformCoord(XMVectorSet(ndcXY[i][0], ndcXY[i][1], 1.0f, 0.0f), inverseViewProjection);
			corners.farCorners[i] = XMVector3TransformCoord(XMVectorSet(ndcXY[i][0], ndcXY[i][1], 0.0f, 0.0f), inverseViewProjection);
		}
		return corners;
	}

	// snapshot builder 규약: position.w <= 0이면 directional, xyz = 빛이 진행하는 방향
	bool findDirectionalLightDirection(JArrayView<const JFrameLightSnapshot::Item> lightItems, XMVECTOR& outDirection)
	{
		for (const JFrameLightSnapshot::Item& item : lightItems)
		{
			if (item.position.w > 0.0f)
			{
				continue;
			}

			const XMVECTOR direction = XMVectorSet(item.position.x, item.position.y, item.position.z, 0.0f);
			if (XMVectorGetX(XMVector3LengthSq(direction)) > 0.0001f)
			{
				outDirection = XMVector3Normalize(direction);
				return true;
			}
		}
		return false;
	}

	uint32 findCBufferRootParameterIndex(Render::JShader* shader, const char* name)
	{
		if (shader == nullptr || name == nullptr)
		{
			return static_cast<uint32>(-1);
		}

		const uint32 nameHash = JHashFunction::StrCrc32(name);
		for (uint32 i = 0; i < static_cast<uint32>(shader->bindingInfo.cBuffers.size()); ++i)
		{
			if (shader->bindingInfo.cBuffers[i].nameHash == nameHash)
			{
				return i;
			}
		}
		return static_cast<uint32>(-1);
	}
}

JShadowPass::~JShadowPass()
{
	delete _pipeline;
	delete _shader;
}

bool JShadowPass::ensureResources(const JRenderPassContext& context)
{
	if (_shader != nullptr && _pipeline != nullptr)
	{
		return true;
	}

	if (context.renderContext == nullptr)
	{
		return false;
	}

	if (_shader == nullptr)
	{
		// light-space에서도 결국 depth만 쓰면 되므로 depth prepass 셰이더를 그대로 재사용한다.
		const std::string shaderPath = get_Engine_Shader_Path() + "\\depth_prepass.hlsl";
		_shader = context.renderContext->CreateShader(shaderPath);
		if (_shader != nullptr)
		{
			_perObjectRootParameterIndex = findCBufferRootParameterIndex(_shader, "PerObject");
			_perFrameRootParameterIndex = findCBufferRootParameterIndex(_shader, "PerFrame");
		}
	}

	if (_shader != nullptr && _pipeline == nullptr)
	{
		_pipeline = context.renderContext->CreatePipeline(
			_shader,
			false,
			true,
			false,
			false,
			{},
			true,
			D3D12_DEPTH_WRITE_MASK_ALL,
			D3D12_COMPARISON_FUNC_GREATER,
			false);
	}

	return _shader != nullptr && _pipeline != nullptr;
}

void JShadowPass::Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc)
{
	_lastStats = {};
	_lastStats.name = GetName();

	JShadowMap* shadowMap = context.shadowMap;
	if (context.commandQueue == nullptr || context.renderDB == nullptr || shadowMap == nullptr || !shadowMap->IsValid())
	{
		return;
	}

	shadowMap->cascadeData = {};
	if (!ensureResources(context))
	{
		return;
	}
	context.commandQueue->SetPipeline(_pipeline);
	context.commandQueue->SetGraphicResources(_shader);

	XMVECTOR lightDirection = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
	const bool hasDirectional = findDirectionalLightDirection(frameDesc.lightItems, lightDirection);

	const JShadowMap::Desc& desc = shadowMap->GetDesc();
	const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(desc.resolution), static_cast<float>(desc.resolution), 0.0f, 1.0f };
	const D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(desc.resolution), static_cast<LONG>(desc.resolution) };

	// 카메라 프러스텀 코너로부터 cascade 분할 구간을 계산함.
	// (frameDesc에 near/far가 없어도 inverseViewProjection만으로 전부 유도 가능)
	const FrustumCorners corners = unprojectFrustum(frameDesc.cameraInverseViewProjection);
	const XMVECTOR cameraPosition = XMVectorSet(frameDesc.cameraWorldPosition.x, frameDesc.cameraWorldPosition.y, frameDesc.cameraWorldPosition.z, 1.0f);
	XMVECTOR nearCenter = XMVectorZero();
	XMVECTOR farCenter = XMVectorZero();
	for (uint32 i = 0; i < 4; ++i)
	{
		nearCenter = XMVectorAdd(nearCenter, corners.nearCorners[i]);
		farCenter = XMVectorAdd(farCenter, corners.farCorners[i]);
	}
	nearCenter = XMVectorScale(nearCenter, 0.25f);
	farCenter = XMVectorScale(farCenter, 0.25f);

	const float nearDistance = XMVectorGetX(XMVector3Length(XMVectorSubtract(nearCenter, cameraPosition)));
	const float farDistance = XMVectorGetX(XMVector3Length(XMVectorSubtract(farCenter, cameraPosition)));
	const float frustumDepth = std::max(farDistance - nearDistance, 0.001f);
	const float shadowDistance = std::min(desc.maxShadowDistance, farDistance);

	for (uint32 cascade = 0; cascade < JShadowMap::CASCADE_COUNT; ++cascade)
	{
		// directional light가 없어도 슬라이스는 clear해 둔다. (lighting pass가 SRV로 바인딩하므로)
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = shadowMap->GetDSVHandle(cascade);
		context.commandQueue->BeginDepthPass(&dsvHandle, 0, true);
		context.commandQueue->SetViewports(1, &viewport);
		context.commandQueue->SetScissorRects(1, &scissorRect);

		if (!hasDirectional || frameDesc.drawItemCache == nullptr)
		{
			context.commandQueue->EndRenderPass();
			continue;
		}

		// cascade가 덮는 카메라 거리 구간 → 프러스텀 코너 보간 파라미터 (선형 보간 = view 거리 선형)
		const float beginDistance = cascade == 0 ? nearDistance : shadowDistance * CASCADE_SPLIT_RATIOS[cascade - 1];
		const float endDistance = shadowDistance * CASCADE_SPLIT_RATIOS[cascade];
		const float tBegin = std::clamp((beginDistance - nearDistance) / frustumDepth, 0.0f, 1.0f);
		const float tEnd = std::clamp((endDistance - nearDistance) / frustumDepth, 0.0f, 1.0f);

		// 슬라이스 8개 코너를 감싸는 bounding sphere로 light-space 직교 볼륨을 잡음
		XMVECTOR sliceCorners[8];
		XMVECTOR sliceCenter = XMVectorZero();
		for (uint32 i = 0; i < 4; ++i)
		{
			const XMVECTOR edge = XMVectorSubtract(corners.farCorners[i], corners.nearCorners[i]);
			sliceCorners[i] = XMVectorAdd(corners.nearCorners[i], XMVectorScale(edge, tBegin));
			sliceCorners[i + 4] = XMVectorAdd(corners.nearCorners[i], XMVectorScale(edge, tEnd));
			sliceCenter = XMVectorAdd(sliceCenter, XMVectorAdd(sliceCorners[i], sliceCorners[i + 4]));
		}
		sliceCenter = XMVectorScale(sliceCenter, 1.0f / 8.0f);

		float radius = 0.0f;
		for (uint32 i = 0; i < 8; ++i)
		{
			radius = std::max(radius, XMVectorGetX(XMVector3Length(XMVectorSubtract(sliceCorners[i], sliceCenter))));
		}
		radius = std::max(radius, 1.0f);

		// 슬라이스 뒤(라이트 쪽)에 있는 caster도 depth에 담기도록 여유를 둠
		const float casterMargin = radius * 2.0f;
		const XMVECTOR up = std::abs(XMVectorGetY(lightDirection)) > 0.99f ? XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f) : XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		const XMVECTOR eye = XMVectorSubtract(sliceCenter, XMVectorScale(lightDirection, radius + casterMargin));
		const XMMATRIX lightView = XMMatrixLookToLH(eye, lightDirection, up);
		// 엔진 전역 reverse-Z(GREATER, clear 0) 규약에 맞춰 near/far를 스왑해 가까운 caster가 큰 depth를 갖게 함
		const float zFar = casterMargin + radius * 2.0f;
		const XMMATRIX lightProjection = XMMatrixOrthographicOffCenterLH(-radius, radius, -radius, radius, zFar, 0.0f);
		const XMMATRIX viewProjection = XMMatrixMultiply(lightView, lightProjection);

		XMStoreFloat4x4(&shadowMap->cascadeData.viewProjections[cascade], XMMatrixTranspose(viewProjection));
		shadowMap->cascadeData.splitDistances[cascade] = endDistance;
		shadowMap->cascadeData.depthBiases[cascade] = desc.depthBiasWorld / zFar;

		JPerFrameConstants cascadeConstants{};
		XMStoreFloat4x4(&cascadeConstants.viewProjection, XMMatrixTranspose(viewProjection));
		const D3D12_GPU_VIRTUAL_ADDRESS cascadeGpuAddress = context.commandQueue->UploadFrameConstantBuffer(&cascadeConstants, sizeof(cascadeConstants));
		if (_perFrameRootParameterIndex != static_cast<uint32>(-1))
		{
			context.commandQueue->SetGraphicsRootConstantBufferView(_perFrameRootParameterIndex, cascadeGpuAddress);
		}

		for (uint32 drawItemIndex : frameDesc.opaqueDrawItemIndices)
		{
			if (drawItemIndex >= frameDesc.drawItemCache->drawItems.size())
			{
				continue;
			}

			const JDrawItem& drawItem = frameDesc.drawItemCache->drawItems[drawItemIndex];
			const JRenderDB::ResolvedDrawResources resources = context.renderDB->ResolveDrawResources(drawItem);
			if (!resources.IsValid())
			{
				++_lastStats.skippedDrawCount;
				continue;
			}

			const JTransformResource* transformResource = resources.transform;
			if (transformResource == nullptr)
			{
				++_lastStats.skippedDrawCount;
				continue;
			}

			if (_perObjectRootParameterIndex != static_cast<uint32>(-1))
			{
				const D3D12_GPU_VIRTUAL_ADDRESS objectGpuAddress = context.commandQueue->UploadFrameConstantBuffer(&transformResource->constants, sizeof(transformResource->constants));
				context.commandQueue->SetGraphicsRootConstantBufferView(_perObjectRootParameterIndex, objectGpuAddress);
			}
			context.commandQueue->BindVertexBuffer(resources.mesh);
			const uint32 indexCount = drawItem.indexCount != 0 ? drawItem.indexCount : static_cast<uint32>(resources.mesh->indexSize);
			context.commandQueue->DrawIndexed(indexCount, 1, drawItem.startIndex, 0, 0);
			++_lastStats.drawCallCount;
		}

		context.commandQueue->EndRenderPass();
	}

	shadowMap->cascadeData.hasDirectionalLight = hasDirectional;
}

J_ENGINE_END
