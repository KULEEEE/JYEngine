#pragma once
#ifndef __J_DEPTH_PASS_H__
#define __J_DEPTH_PASS_H__

#include "engine/render/JRenderPass.h"

namespace J::Render
{
	class JPipeline;
	class JShader;
}

J_ENGINE_BEGIN

class JDepthPass final : public JRenderPass
{
public:
	~JDepthPass() override;

	const char* GetName() const override { return "DepthPass"; }
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
