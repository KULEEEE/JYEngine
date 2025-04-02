#pragma once

#ifndef __J_RENDER_CONTEXT_H__
#define __J_RENDER_CONTEXT_H__

#include "precompile.h"
#include "engine/JDevice.h"
#include "engine/JRenderDefinition.h"
#include "engine/asset/JShader.h"

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
	
	JShader* CreateShader(const std::string& path)
	{
		JShader* shader = new JShader(path);
		shader->CompileShader();
		return shader;
	}
	void DestroyShader(JShader* shader) { delete shader; }

	JPipeline* CreatePipeline(JShader* shader)
	{
		ComPtr<ID3D12Device> device = _device->GetDevice();

		JPipeline* pipeline = new JPipeline();

		D3D12_GRAPHICS_PIPELINE_STATE_DESC& pipelineDesc = pipeline->pipelineDesc;

		// 이 부분은 나중에 shader에서 가져오도록 해야함
		D3D12_INPUT_ELEMENT_DESC desc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			//{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		pipelineDesc.InputLayout = { desc, _countof(desc) };

		pipelineDesc.pRootSignature = shader->GetRootSignature()->signature.Get();
		pipelineDesc.VS = shader->GetByteCode()[0];
		pipelineDesc.PS = shader->GetByteCode()[1];
		pipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
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
			rootParam.InitAsConstantBufferView(cb.slot); // 각 상수 버퍼를 개별 루트 파라미터로 추가
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

		// 4. 루트 서명 생성
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