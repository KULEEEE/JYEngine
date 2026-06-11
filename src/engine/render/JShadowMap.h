#pragma once
#ifndef __J_SHADOW_MAP_H__
#define __J_SHADOW_MAP_H__

#include "engine/precompile.h"
#include "engine/render/JRenderTarget.h"

J_ENGINE_BEGIN

// directional light용 cascade shadow map.
// depth는 CASCADE_COUNT 슬라이스 Texture2DArray 하나에 cascade별 DSV로 나눠 그린다.
// cascade 행렬/분할 거리는 JShadowPass가 매 프레임 채우고 JLightingPass가 읽는다.
class JShadowMap
{
public:
	static constexpr uint32 CASCADE_COUNT = 4;

	struct Desc
	{
		uint32 resolution = 2048;          // cascade 한 장의 해상도
		float maxShadowDistance = 100.0f;  // 카메라로부터 그림자를 그릴 최대 거리
		float depthBiasWorld = 0.035f;     // world 단위 bias. cascade별로 depth 단위로 환산해 사용함
	};

	struct CascadeData
	{
		XMFLOAT4X4 viewProjections[CASCADE_COUNT] = {}; // cbuffer 업로드용으로 transpose된 상태
		float splitDistances[CASCADE_COUNT] = {};       // 카메라 거리 기준 cascade 경계
		float depthBiases[CASCADE_COUNT] = {};          // reverse-Z depth 단위 bias
		bool hasDirectionalLight = false;
	};

	bool Initialize(const Desc& desc);
	bool IsValid() const;

	const Desc& GetDesc() const { return _desc; }
	JRenderTarget* GetDepthTarget() const { return _depth.get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle(uint32 cascade) const { return _depth != nullptr ? _depth->GetDSVHandle(cascade) : D3D12_CPU_DESCRIPTOR_HANDLE{}; }

	CascadeData cascadeData;

private:
	Desc _desc;
	std::unique_ptr<JRenderTarget> _depth;
};

J_ENGINE_END

#endif
