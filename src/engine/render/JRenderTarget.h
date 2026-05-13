#pragma once
#ifndef __J_RENDER_TARGET_H__
#define __J_RENDER_TARGET_H__

#include "engine/precompile.h"
#include "engine/render/JRenderDefinition.h"

J_ENGINE_BEGIN

class JRenderTarget
{
public:
	struct Desc
	{
		uint32 width = 1;
		uint32 height = 1;
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		JColor clearColor = JColors::DarkGray;
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		bool shaderResource = false;
	};

	JRenderTarget();
	JRenderTarget(uint32 width, uint32 height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM, const JColor& clearColor = JColors::DarkGray);
	explicit JRenderTarget(const Desc& desc);
	JRenderTarget(ID3D12Resource* resource);
	~JRenderTarget();

	bool IsValid() const { return !_rtvResources.empty() && _rtvResources[0] != nullptr; }
	bool IsSwapChainTarget() const { return _isSwapChainTarget; }
	bool OwnsResource() const { return _ownsResource; }
	bool HasShaderResource() const { return _shaderResource && _srvHeap != nullptr; }
	uint32 GetWidth() const { return _width; }
	uint32 GetHeight() const { return _height; }
	DXGI_FORMAT GetFormat() const { return _format; }
	const JColor& GetClearColor() const { return _clearColor; }
	D3D12_RESOURCE_STATES GetResourceState() const { return _resourceState; }
	void SetResourceState(D3D12_RESOURCE_STATES state) { _resourceState = state; }

	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& GetRTVHandle();
	const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& GetRTVHandle() const;
	std::vector<ID3D12Resource*>& GetRTVResource();
	const std::vector<ID3D12Resource*>& GetRTVResource() const;
	ID3D12Resource* GetResource(uint32 index = 0) const;
	Render::JTexture* GetTextureView() { return HasShaderResource() ? &_textureView : nullptr; }
	const Render::JTexture* GetTextureView() const { return HasShaderResource() ? &_textureView : nullptr; }
	ID3D12DescriptorHeap* GetSRVHeap() const { return _srvHeap.Get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUHandle() const { return _srvCPUHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle() const { return _srvGPUHandle; }

private:
	bool CreateOffscreenResource(const Desc& desc);
	bool CreateShaderResourceView(ID3D12Resource* resource);

	std::vector<ComPtr<ID3D12Resource>> _ownedResources;
	std::vector<ID3D12Resource*> _rtvResources;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> _rtvHandles;
	ComPtr<ID3D12DescriptorHeap> _srvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE _srvCPUHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE _srvGPUHandle = {};
	Render::JTexture _textureView;

	uint32 _width = 0;
	uint32 _height = 0;
	DXGI_FORMAT _format = DXGI_FORMAT_UNKNOWN;
	JColor _clearColor = JColors::DarkGray;
	bool _isSwapChainTarget = false;
	bool _ownsResource = false;
	bool _shaderResource = false;
	D3D12_RESOURCE_STATES _resourceState = D3D12_RESOURCE_STATE_COMMON;
};

J_ENGINE_END

#endif
