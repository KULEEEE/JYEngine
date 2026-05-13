#pragma once
#ifndef __J_GBUFFER_PASS_H__
#define __J_GBUFFER_PASS_H__

#include "engine/render/JRenderPass.h"

J_ENGINE_BEGIN

class JGBufferPass final : public JRenderPass
{
public:
	const char* GetName() const override { return "GBufferPass"; }
	void Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc) override;
	const JRenderPassStats& GetLastStats() const override { return _lastStats; }

private:
	JRenderPassStats _lastStats = {};
};

J_ENGINE_END

#endif
