#pragma once
#ifndef __J_GRAPHICRESOURCE_H__
#define __J_GRAPHICRESOURCE_H__

#include "engine/JRenderDefinition.h"

/*#include "engine/asset/JShader.h"*/ namespace J { namespace Render { class JShader; } }

J_RENDER_BEGIN

class JGraphicResource : public JInstantiable
{
public:
	struct ConstantBufferBinding
	{
		std::string name;
		uint32 nameHash = 0;
		uint32 shaderSlot = 0;
		uint32 rootParameterIndex = 0;
		JConstantBuffer* buffer = nullptr;
	};

	struct TextureBinding
	{
		std::string name;
		uint32 nameHash = 0;
		uint32 shaderSlot = 0;
		uint32 rootParameterIndex = 0;
		JTexture* texture = nullptr;
	};

	explicit JGraphicResource(JShader* shader = nullptr);

	void SetShader(JShader* shader);
	JShader* GetShader() const { return _shader; }

	bool SetConstantBuffer(const std::string& name, JConstantBuffer* buffer);
	bool SetConstantBuffer(uint32 nameHash, JConstantBuffer* buffer, const std::string& debugName = "");
	bool SetTexture(const std::string& name, JTexture* texture);
	bool SetTexture(uint32 nameHash, JTexture* texture, const std::string& debugName = "");

	const std::vector<ConstantBufferBinding>& GetConstantBuffers() const { return _constantBuffers; }
	const std::vector<TextureBinding>& GetTextures() const { return _textures; }

	void ClearBindings();

private:
	JShader* _shader = nullptr;
	std::vector<ConstantBufferBinding> _constantBuffers;
	std::vector<TextureBinding> _textures;

	static int32 FindConstantBufferBindingIndex(JShader* shader, const std::string& name);
	static int32 FindConstantBufferBindingIndex(JShader* shader, uint32 nameHash);
	static int32 FindTextureBindingIndex(JShader* shader, const std::string& name);
	static int32 FindTextureBindingIndex(JShader* shader, uint32 nameHash);
};

J_RENDER_END

#endif
