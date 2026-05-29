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
	void renderDrawItems(const JRenderPassContext& context, const JFrameDesc& frameDesc, const std::vector<uint32>& drawItemIndices, D3D12_GPU_VIRTUAL_ADDRESS cameraGpuAddress);
	void renderDrawItem(const JRenderPassContext& context, const JDrawItem& drawItem, D3D12_GPU_VIRTUAL_ADDRESS cameraGpuAddress);

	JRenderPassStats _lastStats = {};
};

J_ENGINE_END

#endif
