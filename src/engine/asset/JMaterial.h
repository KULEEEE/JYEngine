#pragma once
#ifndef __J_MATERIAL_H__
#define __J_MATERIAL_H__

#include "engine/core/JObject.h"
#include "engine/core/JHashFunction.h"

J_ENGINE_BEGIN

class JMaterial : public JObject
{
public:
	struct ConstantBufferParam
	{
		std::string name;
		uint32 nameHash = 0;
		std::vector<uint8> data;
	};

	struct TextureParam
	{
		std::string name;
		uint32 nameHash = 0;
		std::string path;
	};

	void SetShaderPath(const std::string& shaderPath);
	const std::string& GetShaderPath() const { return _shaderPath; }

	void SetAlphaBlendEnabled(bool enabled);
	bool IsAlphaBlendEnabled() const { return _enableAlphaBlend; }
	void SetConstantBufferData(const std::string& name, const void* data, size_t size);
	void SetTexturePath(const std::string& name, const std::string& path);

	const std::vector<ConstantBufferParam>& GetConstantBuffers() const { return _constantBuffers; }
	const std::vector<TextureParam>& GetTextures() const { return _textures; }

	void MarkDirty() { _dirty = true; }
	void ClearDirty() { _dirty = false; }
	bool IsDirty() const { return _dirty; }

private:
	std::string _shaderPath;
	bool _enableAlphaBlend = false;
	std::vector<ConstantBufferParam> _constantBuffers;
	std::vector<TextureParam> _textures;
	bool _dirty = true;
};

J_ENGINE_END

#endif
