#include "engine/render/JGBuffer.h"

J_ENGINE_BEGIN

bool JGBuffer::Initialize(const Desc& desc)
{
	_desc = desc;
	_desc.width = std::max(_desc.width, 1u);
	_desc.height = std::max(_desc.height, 1u);

	_albedo = createTarget(_desc.width, _desc.height, _desc.albedoFormat, JColor(0.0f, 0.0f, 0.0f, 1.0f));
	_normal = createTarget(_desc.width, _desc.height, _desc.normalFormat, JColor(0.5f, 0.5f, 1.0f, 1.0f));
	_material = createTarget(_desc.width, _desc.height, _desc.materialFormat, JColor(0.0f, 0.0f, 0.0f, 1.0f));
	_depth = createDepthTarget(_desc.width, _desc.height, _desc.depthFormat);

	return IsValid();
}

void JGBuffer::Resize(uint32 width, uint32 height)
{
	if (_desc.width == width && _desc.height == height && IsValid())
	{
		return;
	}

	_desc.width = std::max(width, 1u);
	_desc.height = std::max(height, 1u);
	Initialize(_desc);
}

bool JGBuffer::IsValid() const
{
	return _albedo != nullptr && _albedo->IsValid()
		&& _normal != nullptr && _normal->IsValid()
		&& _material != nullptr && _material->IsValid()
		&& _depth != nullptr && _depth->IsValid() && _depth->HasDSV();
}

std::unique_ptr<JRenderTarget> JGBuffer::createTarget(uint32 width, uint32 height, DXGI_FORMAT format, const JColor& clearColor)
{
	JRenderTarget::Desc targetDesc;
	targetDesc.width = width;
	targetDesc.height = height;
	targetDesc.format = format;
	targetDesc.clearColor = clearColor;
	targetDesc.initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	targetDesc.shaderResource = true;
	return std::make_unique<JRenderTarget>(targetDesc);
}

std::unique_ptr<JRenderTarget> JGBuffer::createDepthTarget(uint32 width, uint32 height, DXGI_FORMAT format)
{
	JRenderTarget::Desc targetDesc;
	targetDesc.width = width;
	targetDesc.height = height;
	targetDesc.format = format;
	targetDesc.dsvFormat = format;
	targetDesc.initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	targetDesc.shaderResource = false;
	targetDesc.type = JRenderTarget::Type::Depth;
	targetDesc.clearDepth = 0.0f;
	return std::make_unique<JRenderTarget>(targetDesc);
}

J_ENGINE_END
