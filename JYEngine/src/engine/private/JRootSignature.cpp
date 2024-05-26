#include "engine/JRootSignature.h"
#include "engine/JDevice.h"

J_RENDER_BEGIN

JRootSignature::JRootSignature()
{
}

JRootSignature::~JRootSignature()
{
}

void JRootSignature::Initialize(const JDevice* device)
{
	D3D12_ROOT_SIGNATURE_DESC desc = CD3DX12_ROOT_SIGNATURE_DESC(D3D12_DEFAULT);
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> blobSignature;
	ComPtr<ID3DBlob> blobError;
	::D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blobSignature, &blobError);
	device->GetDevice()->CreateRootSignature(0, blobSignature->GetBufferPointer(), blobSignature->GetBufferSize(), IID_PPV_ARGS(&_signature));
}

J_RENDER_END