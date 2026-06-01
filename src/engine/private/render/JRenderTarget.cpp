#include "engine/render/JRenderTarget.h"

#include "engine/core/JEngineContext.h"
#include "engine/dx12/JDx12Helper.h"
#include "engine/platform/JDevice.h"

#include <iostream>

J_ENGINE_BEGIN

namespace
{
	D3D12_CLEAR_VALUE makeClearValue(DXGI_FORMAT format, const JColor& color)
	{
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = format;
		clearValue.Color[0] = color.r;
		clearValue.Color[1] = color.g;
		clearValue.Color[2] = color.b;
		clearValue.Color[3] = color.a;
		return clearValue;
	}

	D3D12_CLEAR_VALUE makeDepthClearValue(DXGI_FORMAT format, float depth)
	{
		D3D12_CLEAR_VALUE clearValue{};
		clearValue.Format = format;
		clearValue.DepthStencil.Depth = depth;
		clearValue.DepthStencil.Stencil = 0;
		return clearValue;
	}

	bool isDepthType(JRenderTarget::Type type)
	{
		return type == JRenderTarget::Type::Depth || type == JRenderTarget::Type::ShadowDepth;
	}

	bool isColorType(JRenderTarget::Type type)
	{
		return type == JRenderTarget::Type::Color || type == JRenderTarget::Type::SwapChain;
	}

	DXGI_FORMAT resolveFormat(DXGI_FORMAT viewFormat, DXGI_FORMAT resourceFormat)
	{
		return viewFormat != DXGI_FORMAT_UNKNOWN ? viewFormat : resourceFormat;
	}
}

JRenderTarget::JRenderTarget()
	: JRenderTarget(Desc{})
{
}

JRenderTarget::JRenderTarget(uint32 width, uint32 height, DXGI_FORMAT format, const JColor& clearColor)
{
	Desc desc;
	desc.width = width;
	desc.height = height;
	desc.format = format;
	desc.clearColor = clearColor;
	createOffscreenResource(desc);
}

JRenderTarget::JRenderTarget(const Desc& desc)
{
	createOffscreenResource(desc);
}

JRenderTarget::JRenderTarget(ID3D12Resource* resource)
{
	if (resource == nullptr)
	{
		return;
	}

	D3D12_RESOURCE_DESC desc = resource->GetDesc();
	_width = static_cast<uint32>(desc.Width);
	_height = desc.Height;
	_format = desc.Format;
	SetRenderArea(_width, _height);
	_isSwapChainTarget = true;
	_ownsResource = false;
	_shaderResource = false;
	_resourceState = D3D12_RESOURCE_STATE_PRESENT;

	// GetBuffer  AddRef ComPtr  (Attach) .
	//        ResizeBuffers DXGI_ERROR_INVALID_CALL .
	ComPtr<ID3D12Resource> ownedResource;
	ownedResource.Attach(resource);
	_ownedResources.push_back(ownedResource);
	_resources.push_back(resource);
	_rtvResources.push_back(resource);
	_rtvHandles.push_back(GetEngine()->GetDx12Helper()->CreateCPUDescriptorHandle(resource));
}

JRenderTarget::~JRenderTarget()
{
	_resources.clear();
	_rtvResources.clear();
	_rtvHandles.clear();
	_dsvHandles.clear();
	_dsvHeap.Reset();
	_srvHeap.Reset();
	_ownedResources.clear();
}

bool JRenderTarget::createOffscreenResource(const Desc& desc)
{
	if (GetEngine() == nullptr || GetEngine()->GetDevice() == nullptr || GetEngine()->GetDx12Helper() == nullptr)
	{
		std::cerr << "JRenderTarget::createOffscreenResource failed: engine device or dx12 helper is null." << std::endl;
		return false;
	}

	ComPtr<ID3D12Device> device = GetEngine()->GetDevice()->GetDevice();
	if (device == nullptr)
	{
		std::cerr << "JRenderTarget::createOffscreenResource failed: D3D12 device is null." << std::endl;
		return false;
	}

	_width = std::max(desc.width, 1u);
	_height = std::max(desc.height, 1u);
	_format = desc.format;
	_clearColor = desc.clearColor;
	_viewport = desc.viewport;
	_scissorRect = desc.scissorRect;
	if (_viewport.Width <= 0.0f || _viewport.Height <= 0.0f || _scissorRect.right <= _scissorRect.left || _scissorRect.bottom <= _scissorRect.top)
	{
		SetRenderArea(_width, _height);
	}
	_isSwapChainTarget = false;
	_ownsResource = true;
	_shaderResource = desc.shaderResource;
	_resourceState = desc.initialState;
	const Type type = desc.type;
	const uint16 arraySize = std::max<uint16>(desc.arraySize, 1);
	const DXGI_FORMAT rtvFormat = resolveFormat(desc.rtvFormat, desc.format);
	const DXGI_FORMAT dsvFormat = resolveFormat(desc.dsvFormat, desc.format);
	const DXGI_FORMAT srvFormat = resolveFormat(desc.srvFormat, desc.format);

	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
	if (isDepthType(type))
	{
		flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	}
	else
	{
		flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}
	if (!desc.shaderResource)
	{
		flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = _width;
	resourceDesc.Height = _height;
	resourceDesc.DepthOrArraySize = arraySize;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = _format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = flags;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	const DXGI_FORMAT clearFormat = isDepthType(type) ? dsvFormat : rtvFormat;
	D3D12_CLEAR_VALUE clearValue = isDepthType(type)
		? makeDepthClearValue(clearFormat, desc.clearDepth)
		: makeClearValue(clearFormat, _clearColor);
	ComPtr<ID3D12Resource> resource;
	const HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		desc.initialState,
		&clearValue,
		IID_PPV_ARGS(&resource));
	if (FAILED(hr) || resource == nullptr)
	{
		std::cerr << "JRenderTarget::createOffscreenResource failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return false;
	}

	ID3D12Resource* rawResource = resource.Get();
	_ownedResources.push_back(resource);
	_resources.push_back(rawResource);
	auto clearCreatedResources = [this]()
		{
			_resources.clear();
			_rtvResources.clear();
			_rtvHandles.clear();
			_dsvHandles.clear();
			_dsvHeap.Reset();
			_srvHeap.Reset();
			_textureView.texture = nullptr;
			_textureView.srvHeap = nullptr;
			_ownedResources.clear();
		};

	if (isColorType(type) && !createRenderTargetView(device.Get(), rawResource, rtvFormat))
	{
		clearCreatedResources();
		return false;
	}

	if (isDepthType(type) && !createDepthStencilViews(device.Get(), rawResource, dsvFormat, arraySize))
	{
		clearCreatedResources();
		return false;
	}

	if (_shaderResource && !createShaderResourceView(device.Get(), rawResource, srvFormat, arraySize))
	{
		clearCreatedResources();
		return false;
	}

	return true;
}

bool JRenderTarget::createRenderTargetView(ID3D12Device* device, ID3D12Resource* resource, DXGI_FORMAT format)
{
	if (device == nullptr || resource == nullptr)
	{
		return false;
	}

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	_rtvResources.push_back(resource);
	_rtvHandles.push_back(GetEngine()->GetDx12Helper()->CreateCPUDescriptorHandle(resource, &rtvDesc));
	return true;
}

bool JRenderTarget::createDepthStencilViews(ID3D12Device* device, ID3D12Resource* resource, DXGI_FORMAT format, uint16 arraySize)
{
	if (device == nullptr || resource == nullptr)
	{
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = arraySize;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	const HRESULT heapHr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_dsvHeap));
	if (FAILED(heapHr) || _dsvHeap == nullptr)
	{
		std::cerr << "JRenderTarget::createDepthStencilViews heap creation failed. HRESULT=0x" << std::hex << heapHr << std::dec << std::endl;
		return false;
	}

	const uint32 descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	_dsvHandles.resize(arraySize);
	for (uint16 slice = 0; slice < arraySize; ++slice)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(slice) * descriptorSize;

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = format;
		dsvDesc.ViewDimension = arraySize > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DARRAY : D3D12_DSV_DIMENSION_TEXTURE2D;
		if (arraySize > 1)
		{
			dsvDesc.Texture2DArray.FirstArraySlice = slice;
			dsvDesc.Texture2DArray.ArraySize = 1;
			dsvDesc.Texture2DArray.MipSlice = 0;
		}
		else
		{
			dsvDesc.Texture2D.MipSlice = 0;
		}

		device->CreateDepthStencilView(resource, &dsvDesc, handle);
		_dsvHandles[slice] = handle;
	}

	return true;
}

bool JRenderTarget::createShaderResourceView(ID3D12Device* device, ID3D12Resource* resource, DXGI_FORMAT format, uint16 arraySize)
{
	if (device == nullptr || resource == nullptr)
	{
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	const HRESULT heapHr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_srvHeap));
	if (FAILED(heapHr) || _srvHeap == nullptr)
	{
		std::cerr << "JRenderTarget::createShaderResourceView heap creation failed. HRESULT=0x" << std::hex << heapHr << std::dec << std::endl;
		return false;
	}

	_srvCPUHandle = _srvHeap->GetCPUDescriptorHandleForHeapStart();
	_srvGPUHandle = {};

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = format;
	srvDesc.ViewDimension = arraySize > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DARRAY : D3D12_SRV_DIMENSION_TEXTURE2D;
	if (arraySize > 1)
	{
		srvDesc.Texture2DArray.MostDetailedMip = 0;
		srvDesc.Texture2DArray.MipLevels = 1;
		srvDesc.Texture2DArray.FirstArraySlice = 0;
		srvDesc.Texture2DArray.ArraySize = arraySize;
	}
	else
	{
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
	}
	device->CreateShaderResourceView(resource, &srvDesc, _srvCPUHandle);

	_textureView.texture = resource;
	_textureView.srvHeap = _srvHeap.Get();
	return true;
}

void JRenderTarget::SetRenderArea(uint32 width, uint32 height)
{
	const uint32 renderWidth = std::max(width, 1u);
	const uint32 renderHeight = std::max(height, 1u);
	_viewport = { 0, 0, static_cast<float>(renderWidth), static_cast<float>(renderHeight), 0, 1 };
	_scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(renderWidth), static_cast<LONG>(renderHeight));
}

std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& JRenderTarget::GetRTVHandle()
{
	return _rtvHandles;
}

const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& JRenderTarget::GetRTVHandle() const
{
	return _rtvHandles;
}

std::vector<ID3D12Resource*>& JRenderTarget::GetRTVResource()
{
	return _rtvResources;
}

const std::vector<ID3D12Resource*>& JRenderTarget::GetRTVResource() const
{
	return _rtvResources;
}

D3D12_CPU_DESCRIPTOR_HANDLE JRenderTarget::GetDSVHandle(uint32 slice) const
{
	return slice < _dsvHandles.size() ? _dsvHandles[slice] : D3D12_CPU_DESCRIPTOR_HANDLE{};
}

ID3D12Resource* JRenderTarget::GetResource(uint32 index) const
{
	return index < _resources.size() ? _resources[index] : nullptr;
}

J_ENGINE_END
