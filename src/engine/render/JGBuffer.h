#pragma once
#ifndef __J_GBUFFER_H__
#define __J_GBUFFER_H__

#include "engine/precompile.h"
#include "engine/render/JRenderTarget.h"

J_ENGINE_BEGIN

class JGBuffer
{
public:
	struct Desc
	{
		uint32 width = 1;
		uint32 height = 1;
		DXGI_FORMAT albedoFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT normalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		DXGI_FORMAT materialFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;
	};

	bool Initialize(const Desc& desc);
	void Resize(uint32 width, uint32 height);

	bool IsValid() const;
	uint32 GetWidth() const { return _desc.width; }
	uint32 GetHeight() const { return _desc.height; }

	JRenderTarget* GetAlbedoTarget() const { return _albedo.get(); }
	JRenderTarget* GetNormalTarget() const { return _normal.get(); }
	JRenderTarget* GetMaterialTarget() const { return _material.get(); }
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return _depth != nullptr ? _depth->GetDSVHandle() : D3D12_CPU_DESCRIPTOR_HANDLE{}; }

	const Desc& GetDesc() const { return _desc; }

private:
	static std::unique_ptr<JRenderTarget> createTarget(uint32 width, uint32 height, DXGI_FORMAT format, const JColor& clearColor);
	static std::unique_ptr<JRenderTarget> createDepthTarget(uint32 width, uint32 height, DXGI_FORMAT format);

	Desc _desc;
	std::unique_ptr<JRenderTarget> _albedo;
	std::unique_ptr<JRenderTarget> _normal;
	std::unique_ptr<JRenderTarget> _material;
	std::unique_ptr<JRenderTarget> _depth;
};

J_ENGINE_END

#endif
