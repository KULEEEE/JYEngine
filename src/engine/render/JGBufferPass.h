#pragma once
#ifndef __J_GBUFFER_PASS_H__
#define __J_GBUFFER_PASS_H__

#include "engine/render/JRenderPass.h"

namespace J
{
	namespace Render
	{
		class JPipeline;
		class JShader;
	}
}

J_ENGINE_BEGIN

class JGBufferPass final : public JRenderPass
{
public:
	~JGBufferPass() override;

	const char* GetName() const override { return "GBufferPass"; }
	void Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc) override;
	const JRenderPassStats& GetLastStats() const override { return _lastStats; }

private:
	bool ensureResources(const JRenderPassContext& context);

	J::Render::JShader* _shader = nullptr;
	J::Render::JPipeline* _pipeline = nullptr;
	JRenderPassStats _lastStats = {};
};

J_ENGINE_END

#endif
