#pragma once
#ifndef __J_SSAO_PASS_H__
#define __J_SSAO_PASS_H__

#include "engine/render/JRenderPass.h"

/*#include "engine/asset/JShader.h"*/ namespace J { namespace Render { class JShader; struct JPipeline; } }

J_ENGINE_BEGIN

// 화면 공간 AO를 단일 채널 타겟(context.ssaoTarget)에 렌더한다.
// LightingPass가 이 결과를 블러해서 occlusion으로 사용한다.
class JSSAOPass final : public JRenderPass
{
public:
	~JSSAOPass() override;

	const char* GetName() const override { return "SSAOPass"; }
	void Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc) override;
	const JRenderPassStats& GetLastStats() const override { return _lastStats; }

private:
	bool ensureResources(const JRenderPassContext& context);

	JRenderPassStats _lastStats = {};
	Render::JShader* _shader = nullptr;
	Render::JPipeline* _pipeline = nullptr;
};

J_ENGINE_END

#endif
