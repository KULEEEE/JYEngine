#pragma once
#ifndef __J_DESCRIPTOR_HEAP_H__
#define __J_DESCRIPTOR_HEAP_H__

#include "precompile.h"

/*#include "engine/JDevice.h"*/ namespace J { namespace Render { class JDevice; } }

J_RENDER_BEGIN

class JDescriptorHeap
{
public:
	JDescriptorHeap() = delete;
	JDescriptorHeap(const JDevice* device, const uint8& descriptorCount, const uint32& key);
	~JDescriptorHeap();

	inline ID3D12DescriptorHeap* Get() const { return _descriptorHeap.Get(); }

	bool isUse = false;

private:

	void initialize(const JDevice* device, const uint8& descriptorCount);

	ComPtr<ID3D12DescriptorHeap> _descriptorHeap;

	const uint32 _instanceID;
};

J_RENDER_END

#endif // __J_DESCRIPTOR_HEAP_H__