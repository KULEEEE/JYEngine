#pragma once

#ifndef __J_RENDER_CONTEXT_H__
#define __J_RENDER_CONTEXT_H__

#include "engine/precompile.h"
#include "engine/platform/JDevice.h"
#include "engine/render/JRenderDefinition.h"
#include "engine/asset/JShader.h"
#include <iostream>
#include <wincodec.h>

J_RENDER_BEGIN

class JRenderContext
{
public:
	JRenderContext() = delete;
	JRenderContext(JDevice* device);
	~JRenderContext();

	template<typename T>
	JVertexBuffer* CreateVertexBuffer(const std::vector<T>& data, size_t vertexCount)
	{
		if (data.empty() || vertexCount == 0)
		{
			return nullptr;
		}

		size_t size = sizeof(T) * data.size();

		ComPtr<ID3D12Device> device = _device->GetDevice();

		JVertexBuffer* vBuffer = new JVertexBuffer();
		ID3D12Resource*& buffer = vBuffer->buffer;
		D3D12_VERTEX_BUFFER_VIEW& view = vBuffer->view;

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

		HRESULT hr = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&buffer));
		if (FAILED(hr) || buffer == nullptr)
		{
			delete vBuffer;
			return nullptr;
		}

		void* pData = nullptr;
		hr = buffer->Map(0, nullptr, &pData);
		if (FAILED(hr) || pData == nullptr)
		{
			vBuffer->Destroy();
			delete vBuffer;
			return nullptr;
		}
		memcpy(pData, data.data(), size);
		(buffer)->Unmap(0, nullptr);

		view.BufferLocation = (buffer)->GetGPUVirtualAddress();
		view.StrideInBytes = static_cast<uint32>(size / vertexCount);
		view.SizeInBytes = static_cast<uint32>(size);

		return vBuffer;
	}

	JIndexBuffer* CreateIndexBuffer(const std::vector<uint32>& data)
	{
		if (data.empty())
		{
			return nullptr;
		}

		size_t size = sizeof(uint32) * data.size();
		ComPtr<ID3D12Device> device = _device->GetDevice();

		JIndexBuffer* iBuffer = new JIndexBuffer();
		ID3D12Resource*& buffer = iBuffer->buffer;
		D3D12_INDEX_BUFFER_VIEW& view = iBuffer->view;

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

		HRESULT hr = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&buffer));
		if (FAILED(hr) || buffer == nullptr)
		{
			delete iBuffer;
			return nullptr;
		}

		void* pData = nullptr;
		hr = buffer->Map(0, nullptr, &pData);
		if (FAILED(hr) || pData == nullptr)
		{
			iBuffer->Destroy();
			delete iBuffer;
			return nullptr;
		}
		memcpy(pData, data.data(), size);
		(buffer)->Unmap(0, nullptr);

		view.BufferLocation = (buffer)->GetGPUVirtualAddress();
		view.Format = DXGI_FORMAT_R32_UINT;
		view.SizeInBytes = static_cast<uint32>(size);

		return iBuffer;
	}
	
	void DestroyVertexBuffer(JVertexBuffer* buffer)
	{
		if (buffer == nullptr)
		{
			return;
		}

		buffer->Destroy();
		delete buffer;
	}

	void DestroyIndexBuffer(JIndexBuffer* buffer)
	{
		if (buffer == nullptr)
		{
			return;
		}

		buffer->Destroy();
		delete buffer;
	}

	JConstantBuffer* CreateConstantBuffer(void* buffer, size_t size)
	{
		ComPtr<ID3D12Device> device = _device->GetDevice();

		size_t bufferSize = (size + 255) & ~255;

		JConstantBuffer* cBuffer = new JConstantBuffer();

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
		
		HRESULT hr = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&cBuffer->buffer)
		);
		
		void* pData;
		D3D12_RANGE range = {};
		cBuffer->buffer->Map(0, &range, &pData);

		::memset(pData, 0, bufferSize);
		::memcpy(pData, buffer, size);

		cBuffer->buffer->Unmap(0, nullptr);

		return cBuffer;
	}

	void DestroyConstantBuffer(JConstantBuffer* buffer)
	{
		if (buffer == nullptr)
		{
			return;
		}

		buffer->Destroy();
		delete buffer;
	}

	JTexture* CreateSolidColorTexture(const JColor& color)
	{
		ComPtr<ID3D12Device> device = _device->GetDevice();
		if (device == nullptr)
		{
			return nullptr;
		}

		JTexture* texture = new JTexture();

		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = 1;
		textureDesc.Height = 1;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
		HRESULT hr = device->CreateCommittedResource(
			&defaultHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&texture->texture));
		if (FAILED(hr) || texture->texture == nullptr)
		{
			delete texture;
			return nullptr;
		}

		const uint8 pixelData[4] =
		{
			static_cast<uint8>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f),
			static_cast<uint8>(std::clamp(color.a, 0.0f, 1.0f) * 255.0f)
		};

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		UINT rowCount = 0;
		UINT64 rowSizeInBytes = 0;
		UINT64 uploadBufferSize = 0;
		device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &rowCount, &rowSizeInBytes, &uploadBufferSize);

		ComPtr<ID3D12Resource> uploadBuffer;
		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		hr = device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer));
		if (FAILED(hr) || uploadBuffer == nullptr)
		{
			delete texture;
			return nullptr;
		}

		void* mappedData = nullptr;
		D3D12_RANGE readRange = {};
		hr = uploadBuffer->Map(0, &readRange, &mappedData);
		if (FAILED(hr) || mappedData == nullptr)
		{
			delete texture;
			return nullptr;
		}

		uint8* uploadBytes = static_cast<uint8*>(mappedData) + footprint.Offset;
		memcpy(uploadBytes, pixelData, sizeof(pixelData));
		uploadBuffer->Unmap(0, nullptr);

		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12GraphicsCommandList> commandList;
		ComPtr<ID3D12CommandQueue> commandQueue;
		ComPtr<ID3D12Fence> fence;
		HANDLE fenceEvent = nullptr;

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		CD3DX12_TEXTURE_COPY_LOCATION dst(texture->texture, 0);
		CD3DX12_TEXTURE_COPY_LOCATION src(uploadBuffer.Get(), footprint);
		commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			texture->texture,
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		hr = commandList->Close();
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		ID3D12CommandList* lists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, lists);

		hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (fenceEvent == nullptr)
		{
			delete texture;
			return nullptr;
		}

		commandQueue->Signal(fence.Get(), 1);
		if (fence->GetCompletedValue() < 1)
		{
			fence->SetEventOnCompletion(1, fenceEvent);
			::WaitForSingleObject(fenceEvent, INFINITE);
		}
		::CloseHandle(fenceEvent);

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&texture->srvHeap));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(texture->texture, &srvDesc, texture->srvHeap->GetCPUDescriptorHandleForHeapStart());

		return texture;
	}

	void DestroyTexture(JTexture* texture)
	{
		if (texture == nullptr)
		{
			return;
		}

		texture->Destroy();
		delete texture;
	}
	JTexture* CreateTextureFromFile(const std::string& path)
	{
		ComPtr<ID3D12Device> device = _device->GetDevice();
		if (device == nullptr)
		{
			return nullptr;
		}

		HRESULT coHr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		
		if (FAILED(coHr) && coHr != RPC_E_CHANGED_MODE)
		{
			std::cerr << "CreateTextureFromFile failed: COM initialization failed. path=" << path << std::endl;
			return nullptr;
		}

		ComPtr<IWICImagingFactory> factory;
		HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
		if (FAILED(hr) || factory == nullptr)
		{			std::cerr << "CreateTextureFromFile failed: WIC factory creation failed. path=" << path << std::endl;
			return nullptr;
		}

		std::wstring widePath(path.begin(), path.end());
		ComPtr<IWICBitmapDecoder> decoder;
		hr = factory->CreateDecoderFromFilename(widePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
		if (FAILED(hr) || decoder == nullptr)
		{			std::cerr << "CreateTextureFromFile failed: image decode failed. path=" << path << std::endl;
			return nullptr;
		}

		ComPtr<IWICBitmapFrameDecode> frame;
		hr = decoder->GetFrame(0, &frame);
		if (FAILED(hr) || frame == nullptr)
		{			return nullptr;
		}

		UINT width = 0;
		UINT height = 0;
		frame->GetSize(&width, &height);
		if (width == 0 || height == 0)
		{			return nullptr;
		}

		ComPtr<IWICFormatConverter> converter;
		hr = factory->CreateFormatConverter(&converter);
		if (FAILED(hr) || converter == nullptr)
		{			return nullptr;
		}

		hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
		if (FAILED(hr))
		{			std::cerr << "CreateTextureFromFile failed: pixel format conversion failed. path=" << path << std::endl;
			return nullptr;
		}

		const UINT srcRowPitch = width * 4;
		std::vector<uint8> pixels(static_cast<size_t>(srcRowPitch) * height);
		hr = converter->CopyPixels(nullptr, srcRowPitch, static_cast<UINT>(pixels.size()), pixels.data());
		if (FAILED(hr))
		{			return nullptr;
		}

		JTexture* texture = new JTexture();
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		textureDesc.Width = width;
		textureDesc.Height = height;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
		hr = device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture->texture));
		if (FAILED(hr) || texture->texture == nullptr)
		{
			delete texture;			return nullptr;
		}

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		UINT rowCount = 0;
		UINT64 rowSizeInBytes = 0;
		UINT64 uploadBufferSize = 0;
		device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &rowCount, &rowSizeInBytes, &uploadBufferSize);

		ComPtr<ID3D12Resource> uploadBuffer;
		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		hr = device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));
		if (FAILED(hr) || uploadBuffer == nullptr)
		{
			delete texture;			return nullptr;
		}

		void* mappedData = nullptr;
		D3D12_RANGE readRange = {};
		hr = uploadBuffer->Map(0, &readRange, &mappedData);
		if (FAILED(hr) || mappedData == nullptr)
		{
			delete texture;			return nullptr;
		}

		uint8* uploadBytes = static_cast<uint8*>(mappedData) + footprint.Offset;
		for (UINT row = 0; row < height; ++row)
		{
			memcpy(uploadBytes + static_cast<size_t>(row) * footprint.Footprint.RowPitch, pixels.data() + static_cast<size_t>(row) * srcRowPitch, srcRowPitch);
		}
		uploadBuffer->Unmap(0, nullptr);

		ComPtr<ID3D12CommandAllocator> allocator;
		ComPtr<ID3D12GraphicsCommandList> commandList;
		ComPtr<ID3D12CommandQueue> commandQueue;
		ComPtr<ID3D12Fence> fence;
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
		if (FAILED(hr))
		{
			delete texture;			return nullptr;
		}

		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
		if (FAILED(hr))
		{
			delete texture;			return nullptr;
		}

		hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
		if (FAILED(hr))
		{
			delete texture;			return nullptr;
		}

		CD3DX12_TEXTURE_COPY_LOCATION dst(texture->texture, 0);
		CD3DX12_TEXTURE_COPY_LOCATION src(uploadBuffer.Get(), footprint);
		commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commandList->ResourceBarrier(1, &barrier);
		hr = commandList->Close();
		if (FAILED(hr))
		{
			delete texture;			return nullptr;
		}

		ID3D12CommandList* lists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, lists);
		hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		if (FAILED(hr))
		{
			delete texture;			return nullptr;
		}

		HANDLE fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (fenceEvent == nullptr)
		{
			delete texture;			return nullptr;
		}

		commandQueue->Signal(fence.Get(), 1);
		if (fence->GetCompletedValue() < 1)
		{
			fence->SetEventOnCompletion(1, fenceEvent);
			::WaitForSingleObject(fenceEvent, INFINITE);
		}
		::CloseHandle(fenceEvent);

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		hr = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&texture->srvHeap));
		if (FAILED(hr))
		{
			delete texture;			return nullptr;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(texture->texture, &srvDesc, texture->srvHeap->GetCPUDescriptorHandleForHeapStart());

		return texture;
	}
	JShader* CreateShader(const std::string& path)
	{
#ifdef _DEBUG
		std::cout << "Loading shader: " << path << std::endl;
#endif
		JShader* shader = new JShader(path);
		shader->CompileShader();
		if (!shader->IsReady())
		{
			std::cerr << "CreateShader failed: " << path << std::endl;
			delete shader;
			return nullptr;
		}
		return shader;
	}
	void DestroyShader(JShader* shader) { delete shader; }

	JPipeline* CreatePipeline(
		JShader* shader,
		bool enableAlphaBlend = false,
		bool useVertexInput = true,
		bool useNormalInput = false,
		bool useTexcoordInput = false,
		const std::vector<DXGI_FORMAT>& rtvFormats = {},
		bool enableDepth = true,
		D3D12_DEPTH_WRITE_MASK depthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
		D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_GREATER,
		bool useDefaultRtvFormat = true)
	{
		if (shader == nullptr || shader->GetRootSignature() == nullptr)
		{
			std::cerr << "CreatePipeline failed: shader or root signature is null." << std::endl;
			return nullptr;
		}

		ComPtr<ID3D12Device> device = _device->GetDevice();

		JPipeline* pipeline = new JPipeline();

		D3D12_GRAPHICS_PIPELINE_STATE_DESC& pipelineDesc = pipeline->pipelineDesc;

		// ??? ???shader? ?? ???
		D3D12_INPUT_ELEMENT_DESC positionDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_INPUT_ELEMENT_DESC positionNormalDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};
		D3D12_INPUT_ELEMENT_DESC positionNormalTexcoordDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		if (!useVertexInput)
		{
			pipelineDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ nullptr, 0 };
		}
		else if (useNormalInput && useTexcoordInput)
		{
			pipelineDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ positionNormalTexcoordDesc, _countof(positionNormalTexcoordDesc) };
		}
		else if (useNormalInput)
		{
			pipelineDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ positionNormalDesc, _countof(positionNormalDesc) };
		}
		else
		{
			pipelineDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ positionDesc, _countof(positionDesc) };
		}

		pipelineDesc.pRootSignature = shader->GetRootSignature()->signature.Get();
		pipelineDesc.VS = shader->GetByteCode()[0];
		pipelineDesc.PS = shader->GetByteCode()[1];
		pipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		if (enableAlphaBlend)
		{
			D3D12_RENDER_TARGET_BLEND_DESC& blendDesc = pipelineDesc.BlendState.RenderTarget[0];
			blendDesc.BlendEnable = TRUE;
			blendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			blendDesc.BlendOp = D3D12_BLEND_OP_ADD;
			blendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
			blendDesc.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			blendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		}
		pipelineDesc.SampleMask = UINT_MAX;
		pipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		pipelineDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		if (!enableDepth)
		{
			pipelineDesc.DepthStencilState.DepthEnable = FALSE;
			pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		}
		else
		{
			pipelineDesc.DepthStencilState.DepthWriteMask = depthWriteMask;
			pipelineDesc.DepthStencilState.DepthFunc = depthFunc;
		}
		pipelineDesc.DSVFormat = enableDepth ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		const std::vector<DXGI_FORMAT> defaultRtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM };
		const std::vector<DXGI_FORMAT>& activeRtvFormats = (rtvFormats.empty() && useDefaultRtvFormat) ? defaultRtvFormats : rtvFormats;
		pipelineDesc.NumRenderTargets = static_cast<UINT>(activeRtvFormats.size());
		for (UINT rtvIndex = 0; rtvIndex < pipelineDesc.NumRenderTargets && rtvIndex < _countof(pipelineDesc.RTVFormats); ++rtvIndex)
		{
			pipelineDesc.RTVFormats[rtvIndex] = activeRtvFormats[rtvIndex];
			pipelineDesc.BlendState.RenderTarget[rtvIndex].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		}
		pipelineDesc.SampleDesc.Count = 1;

		HRESULT hr = device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipeline->pipelineState));
		if (FAILED(hr)) {
			std::cerr << "CreateGraphicsPipelineState failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
			delete pipeline;
			return nullptr;
		}

		return pipeline;
	}
	void DestroyPipeline(JPipeline* pipeline) { delete pipeline; }

	JRootSignature* CreateRootSignature(JShader* shader)
	{
		JRootSignature* rootSignature = new JRootSignature();
		
		JShader::BindingInfo& bindingInfo = shader->bindingInfo;

		std::vector<CD3DX12_ROOT_PARAMETER> rootParameters;
		std::vector<CD3DX12_DESCRIPTOR_RANGE> descriptorRanges;
		descriptorRanges.reserve(2);

		// 1. Add Root Parameters for Constant Buffers
		for (const auto& cb : bindingInfo.cBuffers) {
			CD3DX12_ROOT_PARAMETER rootParam;
			rootParam.InitAsConstantBufferView(cb.slot); // ?? ?  ???
			rootParameters.push_back(rootParam);
		}

		// 2. Add Root Parameters for Textures
		if (!bindingInfo.textures.empty()) {
			uint32 srvDescriptorCount = 0;
			for (const auto& texture : bindingInfo.textures)
			{
				srvDescriptorCount = std::max(srvDescriptorCount, texture.slot + 1);
			}
			descriptorRanges.emplace_back();
			descriptorRanges.back().Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, srvDescriptorCount, 0);

			CD3DX12_ROOT_PARAMETER srvRootParam;
			srvRootParam.InitAsDescriptorTable(1, &descriptorRanges.back());
			rootParameters.push_back(srvRootParam);
		}

		// 3. Add static samplers. Dynamic sampler descriptor heaps made this path fragile during MRT debugging.
		std::vector<CD3DX12_STATIC_SAMPLER_DESC> staticSamplers;
		for (const auto& sampler : bindingInfo.samplers)
		{
			CD3DX12_STATIC_SAMPLER_DESC samplerDesc(sampler.slot);
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			staticSamplers.push_back(samplerDesc);
		}
		// 4.  ? ?
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(static_cast<UINT>(rootParameters.size()), rootParameters.data(), static_cast<UINT>(staticSamplers.size()), staticSamplers.empty() ? nullptr : staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
		if (FAILED(hr)) {
			if (error) {
				OutputDebugStringA((char*)error->GetBufferPointer());
			}
			return nullptr;
		}

		hr = _device->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature->signature));
		if (FAILED(hr)) {
			return nullptr;
		}


		return rootSignature;
	}
	void DestroyRootSignature(JRootSignature* rootSignature) { delete rootSignature; }

private:
	JDevice* _device;
};

J_RENDER_END

#endif








