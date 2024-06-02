#include "engine/JRenderContext.h"

J_RENDER_BEGIN

JRenderContext::JRenderContext(JDevice* device, JRootSignature* rootSignature)
	: _device(device)
	, _rootSignature(rootSignature)
{
}

JRenderContext::~JRenderContext()
{
}

J_RENDER_END