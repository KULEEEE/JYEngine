#include "engine/render/JGraphicResource.h"

#include "engine/core/JHashFunction.h"
#include "engine/asset/JShader.h"

J_RENDER_BEGIN

JGraphicResource::JGraphicResource(JShader* shader)
{
	SetShader(shader);
}

void JGraphicResource::SetShader(JShader* shader)
{
	_shader = shader;
	ClearBindings();
}

bool JGraphicResource::SetConstantBuffer(const std::string& name, JConstantBuffer* buffer)
{
	return SetConstantBuffer(JHashFunction::StrCrc32(name.c_str()), buffer, name);
}

bool JGraphicResource::SetConstantBuffer(uint32 nameHash, JConstantBuffer* buffer, const std::string& debugName)
{
	if (_shader == nullptr)
	{
		return false;
	}

	const int32 bindingIndex = findConstantBufferBindingIndex(_shader, nameHash);
	if (bindingIndex < 0)
	{
		return false;
	}

	const JShader::BindingInfo::Resource& shaderBinding = _shader->bindingInfo.cBuffers[bindingIndex];
	ConstantBufferBinding binding;
	binding.name = debugName.empty() ? shaderBinding.name : debugName;
	binding.nameHash = nameHash;
	binding.shaderSlot = shaderBinding.slot;
	binding.rootParameterIndex = static_cast<uint32>(bindingIndex);
	binding.buffer = buffer;
	_constantBuffers.push_back(binding);
	return true;
}

bool JGraphicResource::SetConstantBufferAddress(const std::string& name, D3D12_GPU_VIRTUAL_ADDRESS gpuAddress)
{
	return SetConstantBufferAddress(JHashFunction::StrCrc32(name.c_str()), gpuAddress, name);
}

bool JGraphicResource::SetConstantBufferAddress(uint32 nameHash, D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, const std::string& debugName)
{
	if (_shader == nullptr || gpuAddress == 0)
	{
		return false;
	}

	const int32 bindingIndex = findConstantBufferBindingIndex(_shader, nameHash);
	if (bindingIndex < 0)
	{
		return false;
	}

	const JShader::BindingInfo::Resource& shaderBinding = _shader->bindingInfo.cBuffers[bindingIndex];
	ConstantBufferBinding binding;
	binding.name = debugName.empty() ? shaderBinding.name : debugName;
	binding.nameHash = nameHash;
	binding.shaderSlot = shaderBinding.slot;
	binding.rootParameterIndex = static_cast<uint32>(bindingIndex);
	binding.gpuAddress = gpuAddress;
	_constantBuffers.push_back(binding);
	return true;
}

bool JGraphicResource::SetTexture(const std::string& name, JTexture* texture)
{
	return SetTexture(JHashFunction::StrCrc32(name.c_str()), texture, name);
}

bool JGraphicResource::SetTexture(uint32 nameHash, JTexture* texture, const std::string& debugName)
{
	if (_shader == nullptr)
	{
		return false;
	}

	const int32 bindingIndex = findTextureBindingIndex(_shader, nameHash);
	if (bindingIndex < 0)
	{
		return false;
	}

	TextureBinding binding;
	binding.name = debugName.empty() ? _shader->bindingInfo.textures[bindingIndex].name : debugName;
	binding.nameHash = nameHash;
	binding.shaderSlot = _shader->bindingInfo.textures[bindingIndex].slot;
	binding.rootParameterIndex = static_cast<uint32>(_shader->bindingInfo.cBuffers.size());
	binding.texture = texture;
	_textures.push_back(binding);
	return true;
}

void JGraphicResource::AddConstantBufferBinding(uint32 rootParameterIndex, uint32 shaderSlot, JConstantBuffer* buffer)
{
	if (buffer == nullptr)
	{
		return;
	}

	ConstantBufferBinding binding;
	binding.shaderSlot = shaderSlot;
	binding.rootParameterIndex = rootParameterIndex;
	binding.buffer = buffer;
	_constantBuffers.push_back(binding);
}

void JGraphicResource::AddTextureBinding(uint32 rootParameterIndex, uint32 shaderSlot, JTexture* texture)
{
	if (texture == nullptr)
	{
		return;
	}

	TextureBinding binding;
	binding.shaderSlot = shaderSlot;
	binding.rootParameterIndex = rootParameterIndex;
	binding.texture = texture;
	_textures.push_back(binding);
}

void JGraphicResource::ReserveBindings(size_t constantBufferCount, size_t textureCount)
{
	_constantBuffers.reserve(constantBufferCount);
	_textures.reserve(textureCount);
}

void JGraphicResource::ClearBindings()
{
	_constantBuffers.clear();
	_textures.clear();
}

int32 JGraphicResource::findConstantBufferBindingIndex(JShader* shader, const std::string& name)
{
	return findConstantBufferBindingIndex(shader, JHashFunction::StrCrc32(name.c_str()));
}

int32 JGraphicResource::findConstantBufferBindingIndex(JShader* shader, uint32 nameHash)
{
	if (shader == nullptr)
	{
		return -1;
	}

	for (size_t i = 0; i < shader->bindingInfo.cBuffers.size(); ++i)
	{
		if (shader->bindingInfo.cBuffers[i].nameHash == nameHash)
		{
			return static_cast<int32>(i);
		}
	}

	return -1;
}

int32 JGraphicResource::findTextureBindingIndex(JShader* shader, const std::string& name)
{
	return findTextureBindingIndex(shader, JHashFunction::StrCrc32(name.c_str()));
}

int32 JGraphicResource::findTextureBindingIndex(JShader* shader, uint32 nameHash)
{
	if (shader == nullptr)
	{
		return -1;
	}

	for (size_t i = 0; i < shader->bindingInfo.textures.size(); ++i)
	{
		if (shader->bindingInfo.textures[i].nameHash == nameHash)
		{
			return static_cast<int32>(i);
		}
	}

	return -1;
}

J_RENDER_END
