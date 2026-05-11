#pragma once
#ifndef __J_MATERIAL_H__
#define __J_MATERIAL_H__

#include "engine/JObject.h"
#include "engine/JHashFunction.h"

J_ENGINE_BEGIN

class JMaterial : public JObject
{
public:
	struct ConstantBufferParam
	{
		std::string name;
		uint32 nameHash = 0;
		Render::JConstantBuffer* buffer = nullptr;
	};

	struct TextureParam
	{
		std::string name;
		uint32 nameHash = 0;
		Render::JTexture* texture = nullptr;
	};

	void SetConstantBuffer(const std::string& name, Render::JConstantBuffer* buffer);
	void SetTexture(const std::string& name, Render::JTexture* texture);

	const std::vector<ConstantBufferParam>& GetConstantBuffers() const { return _constantBuffers; }
	const std::vector<TextureParam>& GetTextures() const { return _textures; }

	void MarkDirty() { _dirty = true; }
	void ClearDirty() { _dirty = false; }
	bool IsDirty() const { return _dirty; }

private:
	std::vector<ConstantBufferParam> _constantBuffers;
	std::vector<TextureParam> _textures;
	bool _dirty = true;
};

J_ENGINE_END

#endif
