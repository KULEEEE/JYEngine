#pragma once
#ifndef __J_LIGHTING_PASS_H__
#define __J_LIGHTING_PASS_H__

#include "engine/render/JRenderPass.h"

/*#include "engine/asset/JShader.h"*/ namespace J { namespace Render { class JShader; struct JPipeline; } }

J_ENGINE_BEGIN

class JLightingPass final : public JRenderPass
{
public:
	~JLightingPass() override;

	const char* GetName() const override { return "LightingPass"; }
	void Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc) override;
	const JRenderPassStats& GetLastStats() const override { return _lastStats; }

private:
	bool EnsureResources(const JRenderPassContext& context);

	JRenderPassStats _lastStats = {};
	Render::JShader* _shader = nullptr;
	Render::JPipeline* _pipeline = nullptr;
};

J_ENGINE_END

#endif
