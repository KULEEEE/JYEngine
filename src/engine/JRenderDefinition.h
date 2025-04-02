#pragma once
#ifndef __J_RENDER_DEFINITION_H__
#define __J_RENDER_DEFINITION_H__

#include "engine/precompile.h"

#define SWAP_CHAIN_BUFFER_COUNT 2
#define MAX_DESCRIPTOR_COUNT 1215

J_RENDER_BEGIN

struct JWindowInfo
{
	HWND hwnd;  // output Window
	int32 width;
	int32 height;
	bool windowed; // true: Windowed Mode, false: Fullscreen Mode
};

using JViewport = D3D12_VIEWPORT;

class JInstantiable
{
public:
	const uint32  instanceID;

	JInstantiable();
	JInstantiable(const JInstantiable& o);
	JInstantiable(uint32 o);
	~JInstantiable();

	static uint32 MakeID();
	bool operator==(const JInstantiable& o) const;
	bool operator!=(const JInstantiable& o) const;
};

struct JVertexBuffer : public JInstantiable
{
	ID3D12Resource* buffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW view;

	void Destroy()
	{
		if (nullptr != buffer)
		{
			buffer->Release();
			buffer = nullptr;
		}
	}
};

struct JIndexBuffer : public JInstantiable
{
	ID3D12Resource* buffer;
	D3D12_INDEX_BUFFER_VIEW view;

	void Destroy()
	{
		if (nullptr != buffer)
		{
			buffer->Release();
			buffer = nullptr;
		}
	}
};

struct JConstantBuffer : public JInstantiable
{
	ID3D12Resource* buffer;

	void Destroy()
	{
		if (nullptr != buffer)
		{
			buffer->Release();
			buffer = nullptr;
		}
	}
};

struct JTexture : public JInstantiable// TODO: Refactoring
{
	ID3D12Resource* texture;

	void Destroy()
	{
		if (nullptr != texture)	delete texture;
	}
};

struct JDescriptorHeap : public JInstantiable
{
	ID3D12DescriptorHeap* heap;

	void Destroy();
};

struct JGraphicResource : JInstantiable
{
	std::vector<JConstantBuffer> constantBuffers;

};

struct JPipeline
{
	ComPtr<ID3D12PipelineState>			pipelineState;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC  pipelineDesc = {};
};

struct JRootSignature
{
	ComPtr<ID3D12RootSignature> signature;
};
J_RENDER_END
#endif