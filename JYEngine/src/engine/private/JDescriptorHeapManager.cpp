#include "engine/JDescriptorHeapManager.h"

#include "engine/JDevice.h"
#include "engine/JRenderDefinition.h"
#include "engine/JHashFunction.h"

using namespace J::Render;

J_ENGINE_BEGIN
JDescriptorHeapManager::JDescriptorHeapManager(const JDevice* device)
	:_device(device)
{
}

JDescriptorHeapManager::~JDescriptorHeapManager()
{
	for (auto& descriptorHeap : _descriptorHeapMap)
	{
		delete descriptorHeap.second;
	}
}

Render::JDescriptorHeap* JDescriptorHeapManager::Get(const std::vector<Render::JContantBuffer*>& constantBuffers, const std::vector<Render::JTexture*>& textures)
{
	//Make Key
	uint32 key = 0;
	for (auto& constantBuffer : constantBuffers)
	{
		key = HashCombine(key, constantBuffer->instanceID);
	}

	for (auto& texture : textures)
	{
		key = HashCombine(key, texture->instanceID);
	}

	if(_descriptorHeapMap.find(key) != _descriptorHeapMap.end())
	{
		_descriptorHeapMap[key]->isUse = true;
		return _descriptorHeapMap[key];
	}
	else
	{
		uint8 descriptorCount = constantBuffers.size() + textures.size();

		Render::JDescriptorHeap* descriptorHeap = new Render::JDescriptorHeap(_device, descriptorCount, key);
		descriptorHeap->isUse = true;
		
		// descriptorHeap¿¡ contantBuffers Binding

		return descriptorHeap;
	}
}

J_ENGINE_END