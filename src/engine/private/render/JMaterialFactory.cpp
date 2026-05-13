#include "engine/render/JMaterialFactory.h"

#include "engine/render/JRenderContext.h"
#include "engine/asset/JMaterial.h"

#include <iostream>

J_ENGINE_BEGIN

JMaterialFactory::JMaterialFactory(Render::JRenderContext* renderContext)
	: _renderContext(renderContext)
{
}

void JMaterialFactory::Initialize(Render::JRenderContext* renderContext)
{
	_renderContext = renderContext;
}

JMaterial* JMaterialFactory::CreateMaterial(const std::string& shaderPath, bool enableAlphaBlend) const
{
	if (_renderContext == nullptr)
	{
		std::cerr << "JMaterialFactory::CreateMaterial failed: render context is null." << std::endl;
		return nullptr;
	}

	Render::JShader* shader = _renderContext->CreateShader(shaderPath);
	if (shader == nullptr)
	{
		return nullptr;
	}

	Render::JPipeline* pipeline = _renderContext->CreatePipeline(shader, enableAlphaBlend);
	if (pipeline == nullptr)
	{
		delete shader;
		return nullptr;
	}

	JMaterial* material = new JMaterial();
	material->SetShader(shader);
	material->SetPipeline(pipeline);
	return material;
}

Render::JConstantBuffer* JMaterialFactory::CreateConstantBuffer(void* data, size_t size) const
{
	return _renderContext != nullptr ? _renderContext->CreateConstantBuffer(data, size) : nullptr;
}

Render::JTexture* JMaterialFactory::CreateSolidColorTexture(const JColor& color) const
{
	return _renderContext != nullptr ? _renderContext->CreateSolidColorTexture(color) : nullptr;
}

Render::JTexture* JMaterialFactory::CreateTextureFromFile(const std::string& path) const
{
	return _renderContext != nullptr ? _renderContext->CreateTextureFromFile(path) : nullptr;
}

Render::JConstantBuffer* JMaterialFactory::CreateAndSetConstantBuffer(JMaterial* material, const std::string& name, void* data, size_t size) const
{
	if (material == nullptr)
	{
		return nullptr;
	}

	Render::JConstantBuffer* buffer = CreateConstantBuffer(data, size);
	if (buffer != nullptr)
	{
		material->SetConstantBuffer(name, buffer);
	}
	return buffer;
}

Render::JTexture* JMaterialFactory::CreateAndSetSolidColorTexture(JMaterial* material, const std::string& name, const JColor& color) const
{
	if (material == nullptr)
	{
		return nullptr;
	}

	Render::JTexture* texture = CreateSolidColorTexture(color);
	if (texture != nullptr)
	{
		material->SetTexture(name, texture);
	}
	return texture;
}

Render::JTexture* JMaterialFactory::CreateAndSetTextureFromFile(JMaterial* material, const std::string& name, const std::string& path) const
{
	if (material == nullptr)
	{
		return nullptr;
	}

	Render::JTexture* texture = CreateTextureFromFile(path);
	if (texture != nullptr)
	{
		material->SetTexture(name, texture);
	}
	return texture;
}

J_ENGINE_END
