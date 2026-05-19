#pragma once
#ifndef __J_DEBUG_OVERLAY_PASS_H__
#define __J_DEBUG_OVERLAY_PASS_H__

#include "engine/render/JRenderPass.h"

/*#include "engine/asset/JShader.h"*/ namespace J { namespace Render { class JShader; struct JPipeline; struct JVertexBuffer; struct JIndexBuffer; } }

J_ENGINE_BEGIN

class JDebugOverlayPass final : public JRenderPass
{
public:
	~JDebugOverlayPass() override;

	const char* GetName() const override { return "DebugOverlayPass"; }
	void Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc) override;
	const JRenderPassStats& GetLastStats() const override { return _lastStats; }

private:
	bool ensureResources(const JRenderPassContext& context);
	bool ensureOverlayBuffers(const JRenderPassContext& context, const std::vector<float>& positions, const std::vector<uint32>& indices);
	void releaseOwnedBuffers();

	JRenderPassStats _lastStats = {};
	Render::JShader* _shader = nullptr;
	Render::JPipeline* _pipeline = nullptr;
	Render::JVertexBuffer* _vertexBuffer = nullptr;
	Render::JIndexBuffer* _indexBuffer = nullptr;
	size_t _vertexFloatCapacity = 0;
	size_t _indexCapacity = 0;
	std::vector<Render::JVertexBuffer*> _retiredVertexBuffers;
	std::vector<Render::JIndexBuffer*> _retiredIndexBuffers;
};

J_ENGINE_END

#endif
