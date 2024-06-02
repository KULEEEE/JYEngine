#pragma once

#ifndef __J_RENDER_CONTEXT_H__
#define __J_RENDER_CONTEXT_H__

#include "precompile.h"
#include "engine/JDevice.h"
#include "engine/JRootSignature.h"
#include "engine/JRenderDefinition.h"
#include "engine/asset/JShader.h"

J_RENDER_BEGIN

class JRenderContext
{
public:
	JRenderContext() = delete;
	JRenderContext(JDevice* device, JRootSignature* rootSignature);
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
	
	void DestroyVertexBuffer(JVertexBuffer* buffer) { delete buffer; }

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
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		pipelineDesc.InputLayout = { desc, _countof(desc) };

		pipelineDesc.pRootSignature = _rootSignature->GetSignature().Get();
		pipelineDesc.VS = shader->GetByteCode()[0];
		pipelineDesc.PS = shader->GetByteCode()[1];
		pipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pipelineDesc.SampleMask = UINT_MAX;
		pipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
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
private:
	JDevice* _device;
	JRootSignature* _rootSignature;
};

J_RENDER_END

#endif