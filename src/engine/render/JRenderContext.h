#pragma once

#ifndef __J_RENDER_CONTEXT_H__
#define __J_RENDER_CONTEXT_H__

#include "engine/precompile.h"
#include "engine/platform/JDevice.h"
#include "engine/render/JRenderDefinition.h"
#include "engine/asset/JShader.h"
#include <iostream>

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
		size_t size = sizeof(T) * data.size();

		ComPtr<ID3D12Device> device = _device->GetDevice();

		JVertexBuffer* vBuffer = new JVertexBuffer();
		ID3D12Resource*& buffer = vBuffer->buffer;
		D3D12_VERTEX_BUFFER_VIEW& view = vBuffer->view;

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&buffer));

		void* pData;
		(buffer)->Map(0, nullptr, &pData);
		memcpy(pData, data.data(), size);
		(buffer)->Unmap(0, nullptr);

		view.BufferLocation = (buffer)->GetGPUVirtualAddress();
		view.StrideInBytes = static_cast<uint32>(size / vertexCount);
		view.SizeInBytes = static_cast<uint32>(size);

		return vBuffer;
	}

	JIndexBuffer* CreateIndexBuffer(const std::vector<uint32>& data)
	{
		size_t size = sizeof(uint32) * data.size();
		ComPtr<ID3D12Device> device = _device->GetDevice();

		JIndexBuffer* iBuffer = new JIndexBuffer();
		ID3D12Resource*& buffer = iBuffer->buffer;
		D3D12_INDEX_BUFFER_VIEW& view = iBuffer->view;

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&buffer));

		void* pData;
		(buffer)->Map(0, nullptr, &pData);
		memcpy(pData, data.data(), size);
		(buffer)->Unmap(0, nullptr);

		view.BufferLocation = (buffer)->GetGPUVirtualAddress();
		view.Format = DXGI_FORMAT_R32_UINT;
		view.SizeInBytes = static_cast<uint32>(size);

		return iBuffer;
	}
	
	void DestroyVertexBuffer(JVertexBuffer* buffer) { delete buffer; }

	void DestroyIndexBuffer(JIndexBuffer* buffer) { delete buffer; }

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

		::memcpy(pData, buffer, bufferSize);

		cBuffer->buffer->Unmap(0, nullptr);

		return cBuffer;
	}

	void UpdateConstantBuffer(JConstantBuffer* buffer, void* data, size_t size)
	{
		ComPtr<ID3D12Device> device = _device->GetDevice();

		size_t bufferSize = (size + 255) & ~255;

		if (buffer->buffer->GetDesc().Width < bufferSize)
		{
			buffer->buffer->Release();
			buffer->buffer = nullptr;
		}

		if (buffer->buffer == nullptr)
		{
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

			HRESULT hr = device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&buffer->buffer)
			);
		}

		void* pData;
		D3D12_RANGE range = {};
		buffer->buffer->Map(0, &range, &pData);

		::memcpy(pData, data, bufferSize);

		buffer->buffer->Unmap(0, nullptr);
	}

	JTexture* CreateSolidColorTexture(const JColor& color)
	{
		ComPtr<ID3D12Device> device = _device->GetDevice();

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

		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

		HRESULT hr = device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&texture->texture));
		if (FAILED(hr))
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

		hr = texture->texture->WriteToSubresource(0, nullptr, pixelData, sizeof(pixelData), sizeof(pixelData));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
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
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(texture->texture, &srvDesc, texture->srvHeap->GetCPUDescriptorHandleForHeapStart());

		D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
		samplerHeapDesc.NumDescriptors = 1;
		samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		hr = device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&texture->samplerHeap));
		if (FAILED(hr))
		{
			delete texture;
			return nullptr;
		}

		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		device->CreateSampler(&samplerDesc, texture->samplerHeap->GetCPUDescriptorHandleForHeapStart());

		return texture;
	}
	
	JShader* CreateShader(const std::string& path)
	{
		std::cout << "Loading shader: " << path << std::endl;
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

	JPipeline* CreatePipeline(JShader* shader, bool enableAlphaBlend = false, bool useVertexInput = true)
	{
		if (shader == nullptr || shader->GetRootSignature() == nullptr)
		{
			std::cerr << "CreatePipeline failed: shader or root signature is null." << std::endl;
			return nullptr;
		}

		ComPtr<ID3D12Device> device = _device->GetDevice();

		JPipeline* pipeline = new JPipeline();

		D3D12_GRAPHICS_PIPELINE_STATE_DESC& pipelineDesc = pipeline->pipelineDesc;

		// ??Î∂ÄÎ∂ÑÏ? ?òÏ§ë??shader?êÏÑú Í∞Ä?∏Ïò§?ÑÎ°ù ?¥Ïïº??
		D3D12_INPUT_ELEMENT_DESC desc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			//{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		pipelineDesc.InputLayout = useVertexInput ? D3D12_INPUT_LAYOUT_DESC{ desc, _countof(desc) } : D3D12_INPUT_LAYOUT_DESC{ nullptr, 0 };

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
		pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
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

		// 1. Add Root Parameters for Constant Buffers
		for (const auto& cb : bindingInfo.cBuffers) {
			CD3DX12_ROOT_PARAMETER rootParam;
			rootParam.InitAsConstantBufferView(cb.slot); // Í∞??ÅÏàò Î≤ÑÌçºÎ•?Í∞úÎ≥Ñ Î£®Ìä∏ ?åÎùºÎØ∏ÌÑ∞Î°?Ï∂îÍ?
			rootParameters.push_back(rootParam);
		}

		// 2. Add Root Parameters for Textures
		if (!bindingInfo.textures.empty()) {
			CD3DX12_DESCRIPTOR_RANGE srvRange;
			srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<uint32>(bindingInfo.textures.size()), 0);

			CD3DX12_ROOT_PARAMETER srvRootParam;
			srvRootParam.InitAsDescriptorTable(1, &srvRange);
			rootParameters.push_back(srvRootParam);
		}

		// 3. Add Root Parameters for Samplers
		if (!bindingInfo.samplers.empty()) {
			CD3DX12_DESCRIPTOR_RANGE samplerRange;
			samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, static_cast<UINT>(bindingInfo.samplers.size()), 0);

			CD3DX12_ROOT_PARAMETER samplerRootParam;
			samplerRootParam.InitAsDescriptorTable(1, &samplerRange);
			rootParameters.push_back(samplerRootParam);
		}

		// 4. Î£®Ìä∏ ?úÎ™Ö ?ùÏÑ±
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(static_cast<UINT>(rootParameters.size()), rootParameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
