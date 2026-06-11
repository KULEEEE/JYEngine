#pragma once
#ifndef __J_SHADOW_PASS_H__
#define __J_SHADOW_PASS_H__

#include "engine/render/JRenderPass.h"

namespace J::Render
{
	class JPipeline;
	class JShader;
}

J_ENGINE_BEGIN

// 카메라 프러스텀을 거리별 cascade로 쪼개 첫 번째 directional light 기준의
// light-space depth를 shadow map 슬라이스마다 그린다.
class JShadowPass final : public JRenderPass
{
public:
	~JShadowPass() override;

	const char* GetName() const override { return "ShadowPass"; }
	void Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc) override;
	const JRenderPassStats& GetLastStats() const override { return _lastStats; }

private:
	bool ensureResources(const JRenderPassContext& context);

	Render::JShader* _shader = nullptr;
	Render::JPipeline* _pipeline = nullptr;
	uint32 _perObjectRootParameterIndex = static_cast<uint32>(-1);
	uint32 _perFrameRootParameterIndex = static_cast<uint32>(-1);
	JRenderPassStats _lastStats = {};
};

J_ENGINE_END

#endif
