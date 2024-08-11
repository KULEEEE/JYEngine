#include "engine/JDescriptorHeap.h"

#include "engine/JDevice.h"

J_RENDER_BEGIN

JDescriptorHeap::JDescriptorHeap(const JDevice* device, const uint8& descriptorCount, const uint32& key)
	:_instanceID(key)
{
	initialize(device, descriptorCount);
}

void JDescriptorHeap::initialize(const JDevice* device, const uint8& descriptorCount)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = descriptorCount;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.NodeMask = 0;
	device->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_descriptorHeap));
}

J_RENDER_END