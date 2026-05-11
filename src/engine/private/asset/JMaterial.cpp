#include "engine/asset/JMaterial.h"

J_ENGINE_BEGIN

void JMaterial::SetConstantBuffer(const std::string& name, Render::JConstantBuffer* buffer)
{
	const uint32 nameHash = JHashFunction::StrCrc32(name.c_str());
	for (ConstantBufferParam& entry : _constantBuffers)
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

void JMaterial::SetTexture(const std::string& name, Render::JTexture* texture)
{
	const uint32 nameHash = JHashFunction::StrCrc32(name.c_str());
	for (TextureParam& entry : _textures)
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

J_ENGINE_END
