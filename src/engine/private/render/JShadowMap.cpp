#include "engine/render/JShadowMap.h"

J_ENGINE_BEGIN

bool JShadowMap::Initialize(const Desc& desc)
{
	_desc = desc;
	_desc.resolution = std::max(_desc.resolution, 64u);

	JRenderTarget::Desc targetDesc;
	targetDesc.width = _desc.resolution;
	targetDesc.height = _desc.resolution;
	// lighting pass에서 SampleCmp로 읽으므로 typeless로 만들고 DSV/SRV view format을 분리한다.
	targetDesc.format = DXGI_FORMAT_R32_TYPELESS;
	targetDesc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
	targetDesc.srvFormat = DXGI_FORMAT_R32_FLOAT;
	targetDesc.shaderResource = true;
	targetDesc.arraySize = CASCADE_COUNT;
	targetDesc.initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	targetDesc.type = JRenderTarget::Type::ShadowDepth;
	targetDesc.clearDepth = 0.0f;
	_depth = std::make_unique<JRenderTarget>(targetDesc);

	return IsValid();
}

bool JShadowMap::IsValid() const
{
	return _depth != nullptr && _depth->IsValid() && _depth->HasDSV();
}

J_ENGINE_END
