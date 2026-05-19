#include "engine/asset/JMaterial.h"

J_ENGINE_BEGIN

void JMaterial::SetShaderPath(const std::string& shaderPath)
{
	if (_shaderPath == shaderPath)
	{
		return;
	}

	_shaderPath = shaderPath;
	MarkDirty();
}

void JMaterial::SetAlphaBlendEnabled(bool enabled)
{
	if (_enableAlphaBlend == enabled)
	{
		return;
	}

	_enableAlphaBlend = enabled;
	MarkDirty();
}

void JMaterial::SetConstantBufferData(const std::string& name, const void* data, size_t size)
{
	if (data == nullptr || size == 0)
	{
		return;
	}

	const uint32 nameHash = JHashFunction::StrCrc32(name.c_str());
	for (ConstantBufferParam& entry : _constantBuffers)
	{
		if (entry.nameHash == nameHash)
		{
			entry.data.resize(size);
			memcpy(entry.data.data(), data, size);
			MarkDirty();
			return;
		}
	}

	ConstantBufferParam param;
	param.name = name;
	param.nameHash = nameHash;
	param.data.resize(size);
	memcpy(param.data.data(), data, size);
	_constantBuffers.push_back(std::move(param));
	MarkDirty();
}

void JMaterial::SetTexturePath(const std::string& name, const std::string& path)
{
	const uint32 nameHash = JHashFunction::StrCrc32(name.c_str());
	for (TextureParam& entry : _textures)
	{
		if (entry.nameHash == nameHash)
		{
			entry.path = path;
			MarkDirty();
			return;
		}
	}

	_textures.push_back({ name, nameHash, path });
	MarkDirty();
}

J_ENGINE_END
