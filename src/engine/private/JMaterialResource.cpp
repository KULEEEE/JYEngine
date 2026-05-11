#include "engine/JMaterialResource.h"

#include "engine/JRenderDefinition.h"

J_ENGINE_BEGIN

void JMaterialResource::SetConstantBuffer(const std::string& name, Render::JConstantBuffer* buffer)
{
	const uint32 nameHash = JHashFunction::StrCrc32(name.c_str());
	for (ConstantBufferEntry& entry : _constantBuffers)
	{
		if (entry.nameHash == nameHash)
		{
			entry.buffer = buffer;
			MarkDirty();
			return;
		}
	}

	_constantBuffers.push_back({ name, nameHash, buffer });
	MarkDirty();
}

void JMaterialResource::SetTexture(const std::string& name, Render::JTexture* texture)
{
	const uint32 nameHash = JHashFunction::StrCrc32(name.c_str());
	for (TextureEntry& entry : _textures)
	{
		if (entry.nameHash == nameHash)
		{
			entry.texture = texture;
			MarkDirty();
			return;
		}
	}

	_textures.push_back({ name, nameHash, texture });
	MarkDirty();
}

Render::JConstantBuffer* JMaterialResource::FindConstantBuffer(const std::string& name) const
{
	return FindConstantBuffer(JHashFunction::StrCrc32(name.c_str()));
}

Render::JConstantBuffer* JMaterialResource::FindConstantBuffer(uint32 nameHash) const
{
	for (const ConstantBufferEntry& entry : _constantBuffers)
	{
		if (entry.nameHash == nameHash)
		{
			return entry.buffer;
		}
	}

	return nullptr;
}

Render::JTexture* JMaterialResource::FindTexture(const std::string& name) const
{
	return FindTexture(JHashFunction::StrCrc32(name.c_str()));
}

Render::JTexture* JMaterialResource::FindTexture(uint32 nameHash) const
{
	for (const TextureEntry& entry : _textures)
	{
		if (entry.nameHash == nameHash)
		{
			return entry.texture;
		}
	}

	return nullptr;
}

void JMaterialResource::ClearBindings()
{
	_constantBuffers.clear();
	_textures.clear();
	MarkDirty();
}

J_ENGINE_END
