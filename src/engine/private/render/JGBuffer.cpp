#include "engine/render/JGBuffer.h"

#include "engine/core/JEngineContext.h"
#include "engine/platform/JDevice.h"

#include <iostream>

J_ENGINE_BEGIN

bool JGBuffer::Initialize(const Desc& desc)
{
	_desc = desc;
	_desc.width = std::max(_desc.width, 1u);
	_desc.height = std::max(_desc.height, 1u);

	_albedo = createTarget(_desc.width, _desc.height, _desc.albedoFormat, JColor(0.0f, 0.0f, 0.0f, 1.0f));
	_normal = createTarget(_desc.width, _desc.height, _desc.normalFormat, JColor(0.5f, 0.5f, 1.0f, 1.0f));
	_material = createTarget(_desc.width, _desc.height, _desc.materialFormat, JColor(0.0f, 0.0f, 0.0f, 1.0f));
	if (!createDepthTarget())
	{
		return false;
	}

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
		&& _depthResource != nullptr
		&& _dsvHeap != nullptr;
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

bool JGBuffer::createDepthTarget()
{
	if (GetEngine() == nullptr || GetEngine()->GetDevice() == nullptr)
	{
		return false;
	}

	ComPtr<ID3D12Device> device = GetEngine()->GetDevice()->GetDevice();
	if (device == nullptr)
	{
		return false;
	}

	D3D12_RESOURCE_DESC depthDesc = {};
	depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthDesc.Width = _desc.width;
	depthDesc.Height = _desc.height;
	depthDesc.DepthOrArraySize = 1;
	depthDesc.MipLevels = 1;
	depthDesc.Format = _desc.depthFormat;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = _desc.depthFormat;
	clearValue.DepthStencil.Depth = 0.0f;
	clearValue.DepthStencil.Stencil = 0;

	CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
	const HRESULT resourceHr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&depthDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&_depthResource));
	if (FAILED(resourceHr) || _depthResource == nullptr)
	{
		std::cerr << "JGBuffer::createDepthTarget failed. HRESULT=0x" << std::hex << resourceHr << std::dec << std::endl;
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	const HRESULT heapHr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_dsvHeap));
	if (FAILED(heapHr) || _dsvHeap == nullptr)
	{
		std::cerr << "JGBuffer::createDepthTarget DSV heap failed. HRESULT=0x" << std::hex << heapHr << std::dec << std::endl;
		return false;
	}

	_dsvHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = _desc.depthFormat;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device->CreateDepthStencilView(_depthResource.Get(), &dsvDesc, _dsvHandle);
	_depthState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	return true;
}

J_ENGINE_END
