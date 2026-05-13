#pragma once
#ifndef __J_MATERIAL_FACTORY_H__
#define __J_MATERIAL_FACTORY_H__

#include "engine/precompile.h"

/*#include "engine/render/JRenderContext.h"*/ namespace J { namespace Render { class JRenderContext; } }
/*#include "engine/render/JRenderDefinition.h"*/ namespace J { namespace Render { struct JConstantBuffer; struct JTexture; } }
/*#include "engine/asset/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }

J_ENGINE_BEGIN

class JMaterialFactory
{
public:
	JMaterialFactory() = default;
	explicit JMaterialFactory(Render::JRenderContext* renderContext);

	void Initialize(Render::JRenderContext* renderContext);

	JMaterial* CreateMaterial(const std::string& shaderPath, bool enableAlphaBlend = false) const;
	Render::JConstantBuffer* CreateConstantBuffer(void* data, size_t size) const;
	Render::JTexture* CreateSolidColorTexture(const JColor& color) const;

	Render::JConstantBuffer* CreateAndSetConstantBuffer(JMaterial* material, const std::string& name, void* data, size_t size) const;
	Render::JTexture* CreateAndSetSolidColorTexture(JMaterial* material, const std::string& name, const JColor& color) const;

private:
	Render::JRenderContext* _renderContext = nullptr;
};

J_ENGINE_END

#endif
