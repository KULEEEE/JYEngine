#pragma once
#ifndef __J_DESCRIPTOR_HEAP_MANAGER__H 
#define __J_DESCRIPTOR_HEAP_MANAGER__H

#include "precompile.h"
#include "engine/JDescriptorHeap.h"

/*#include "engine/JDevice.h"*/ namespace J {namespace Render { struct JDevice; }}
/*#include "engine/JRenderDefinition.h"*/ namespace J {namespace Render { struct JContantBuffer; }}
/*#include "engine/JRenderDefinition.h"*/ namespace J {namespace Render { struct JTexture; }}

J_ENGINE_BEGIN

class JDescriptorHeapManager
{
public:

	JDescriptorHeapManager() = delete;
	JDescriptorHeapManager(const Render::JDevice* device);
	~JDescriptorHeapManager();

	//key 값을 내부에서 직접 생성한다.
	Render::JDescriptorHeap* Get(const std::vector<Render::JContantBuffer*>& constantBuffers, const std::vector<Render::JTexture*>& textures);
private:

	const Render::JDevice* _device;

	std::unordered_map<uint32, Render::JDescriptorHeap*> _descriptorHeapMap;
};

J_ENGINE_END
#endif