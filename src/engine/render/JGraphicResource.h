#pragma once
#ifndef __J_GRAPHICRESOURCE_H__
#define __J_GRAPHICRESOURCE_H__

#include "engine/render/JRenderDefinition.h"

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
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
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
	bool SetConstantBufferAddress(const std::string& name, D3D12_GPU_VIRTUAL_ADDRESS gpuAddress);
	bool SetConstantBufferAddress(uint32 nameHash, D3D12_GPU_VIRTUAL_ADDRESS gpuAddress, const std::string& debugName = "");
	bool SetTexture(const std::string& name, JTexture* texture);
	bool SetTexture(uint32 nameHash, JTexture* texture, const std::string& debugName = "");
	void AddConstantBufferBinding(uint32 rootParameterIndex, uint32 shaderSlot, JConstantBuffer* buffer);
	void AddTextureBinding(uint32 rootParameterIndex, uint32 shaderSlot, JTexture* texture);
	void ReserveBindings(size_t constantBufferCount, size_t textureCount);

	const std::vector<ConstantBufferBinding>& GetConstantBuffers() const { return _constantBuffers; }
	const std::vector<TextureBinding>& GetTextures() const { return _textures; }

	void ClearBindings();

private:
	JShader* _shader = nullptr;
	std::vector<ConstantBufferBinding> _constantBuffers;
	std::vector<TextureBinding> _textures;

	static int32 findConstantBufferBindingIndex(JShader* shader, const std::string& name);
	static int32 findConstantBufferBindingIndex(JShader* shader, uint32 nameHash);
	static int32 findTextureBindingIndex(JShader* shader, const std::string& name);
	static int32 findTextureBindingIndex(JShader* shader, uint32 nameHash);
};

J_RENDER_END

#endif
