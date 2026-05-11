#pragma once
#ifndef __J_MATERIAL_RESOURCE_H__
#define __J_MATERIAL_RESOURCE_H__

#include "engine/precompile.h"
#include "engine/JHashFunction.h"

/*#include "engine/JRenderDefinition.h"*/ namespace J { namespace Render { struct JConstantBuffer; struct JPipeline; struct JTexture; } }
/*#include "engine/asset/JShader.h"*/ namespace J { namespace Render { class JShader; } }

J_ENGINE_BEGIN

class JMaterialResource
{
public:
	friend class JRenderDB;

	struct ConstantBufferEntry
	{
		std::string name;
		uint32 nameHash = 0;
		Render::JConstantBuffer* buffer = nullptr;
	};

	struct TextureEntry
	{
		std::string name;
		uint32 nameHash = 0;
		Render::JTexture* texture = nullptr;
	};

	explicit JMaterialResource(uint32 materialID = 0)
		: _materialID(materialID)
	{
	}

	uint32 GetMaterialID() const { return _materialID; }

	Render::JShader* GetShader() const { return _shader; }
	Render::JPipeline* GetPipeline() const { return _pipeline; }

	Render::JConstantBuffer* FindConstantBuffer(const std::string& name) const;
	Render::JConstantBuffer* FindConstantBuffer(uint32 nameHash) const;
	Render::JTexture* FindTexture(const std::string& name) const;
	Render::JTexture* FindTexture(uint32 nameHash) const;

	const std::vector<ConstantBufferEntry>& GetConstantBuffers() const { return _constantBuffers; }
	const std::vector<TextureEntry>& GetTextures() const { return _textures; }

	void MarkDirty() { _dirty = true; }
	void ClearDirty() { _dirty = false; }
	bool IsDirty() const { return _dirty; }

private:
	void SetShader(Render::JShader* shader);
	void SetPipeline(Render::JPipeline* pipeline);
	void SetConstantBuffer(const std::string& name, Render::JConstantBuffer* buffer);
	void SetTexture(const std::string& name, Render::JTexture* texture);
	void ClearBindings();

	uint32 _materialID = 0;
	bool _dirty = true;
	Render::JShader* _shader = nullptr;
	Render::JPipeline* _pipeline = nullptr;
	std::vector<ConstantBufferEntry> _constantBuffers;
	std::vector<TextureEntry> _textures;
};

J_ENGINE_END

#endif
