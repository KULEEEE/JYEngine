#pragma once
#ifndef __J_SCENE_COLOR_PASS_H__
#define __J_SCENE_COLOR_PASS_H__

#include "engine/render/JRenderPass.h"

J_ENGINE_BEGIN

class JSceneColorPass final : public JRenderPass
{
public:
	const char* GetName() const override { return "SceneColorPass"; }
	void Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc) override;
	const JRenderPassStats& GetLastStats() const override { return _lastStats; }

private:
	void renderDrawItems(const JRenderPassContext& context, JCameraHandle camera, const JFrameDesc& frameDesc, const std::vector<uint32>& drawItemIndices);
	void renderDrawItem(const JRenderPassContext& context, JCameraHandle camera, const JDrawItem& drawItem);

	JRenderPassStats _lastStats = {};
};

J_ENGINE_END

#endif
