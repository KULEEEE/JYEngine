#include "engine/JRenderContext.h"

J_RENDER_BEGIN

JRenderContext::JRenderContext(JDevice* device)
	: _device(device)
{
}

JRenderContext::~JRenderContext()
{
}

JVertexBuffer* JRenderContext::CreateVertexBuffer(const void* data, size_t size, size_t vertexCount)
{
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
	memcpy(pData, data, size);
	(buffer)->Unmap(0, nullptr);

	view.BufferLocation = (buffer)->GetGPUVirtualAddress();
	view.StrideInBytes = static_cast<uint32>(size / vertexCount);
	view.SizeInBytes = static_cast<uint32>(size);

	return vBuffer;
}

JShader* JRenderContext::CreateShader(const std::wstring& path)
{
	JShader* shader = new JShader(path);

	shader->CompileShader();

	return shader;
}

J_RENDER_END