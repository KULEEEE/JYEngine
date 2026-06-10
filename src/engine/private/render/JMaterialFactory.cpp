#include "engine/render/JMaterialFactory.h"

#include "engine/asset/JMaterial.h"

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
	JMaterial* material = new JMaterial();
	material->SetShaderPath(shaderPath);
	material->SetAlphaBlendEnabled(enableAlphaBlend);
	return material;
}

void JMaterialFactory::SetConstantBufferData(JMaterial* material, const std::string& name, const void* data, size_t size) const
{
	if (material == nullptr)
	{
		return;
	}

	material->SetConstantBufferData(name, data, size);
}

void JMaterialFactory::SetTexturePath(JMaterial* material, const std::string& name, const std::string& path) const
{
	if (material == nullptr)
	{
		return;
	}

	material->SetTexturePath(name, path);
}

J_ENGINE_END
