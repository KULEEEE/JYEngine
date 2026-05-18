#pragma once
#ifndef __J_FORWARD_OVERLAY_PASS_H__
#define __J_FORWARD_OVERLAY_PASS_H__

#include "engine/render/JRenderPass.h"

J_ENGINE_BEGIN

class JForwardOverlayPass final : public JRenderPass
{
public:
	const char* GetName() const override { return "ForwardOverlayPass"; }
	void Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc) override;
	const JRenderPassStats& GetLastStats() const override { return _lastStats; }

private:
	void RenderDrawItems(const JRenderPassContext& context, JCameraHandle camera, const std::vector<JDrawItem>& drawItems);
	void RenderDrawItem(const JRenderPassContext& context, JCameraHandle camera, const JDrawItem& drawItem);

	JRenderPassStats _lastStats = {};
};

J_ENGINE_END

#endif
