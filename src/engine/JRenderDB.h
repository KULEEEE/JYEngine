#pragma once
#ifndef __J_RENDER_DB_H__
#define __J_RENDER_DB_H__

#include "engine/precompile.h"
#include "engine/JMaterialResource.h"

/*#include "engine/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }
/*#include "engine/JGraphicResource.h"*/ namespace J { namespace Render { class JGraphicResource; class JShader; } }

J_ENGINE_BEGIN

class JRenderDB
{
public:
	struct MaterialResourceRecord
	{
		uint32 materialID = 0;
		JMaterialResource resource;
	};

	JRenderDB() = default;
	~JRenderDB();

	JMaterialResource* FindMaterialResource(uint32 materialID);
	const JMaterialResource* FindMaterialResource(uint32 materialID) const;

	void SyncMaterial(const JMaterial& material);
	void RemoveMaterialResource(uint32 materialID);
	void Clear();

	bool BuildGraphicResource(uint32 materialID, Render::JShader* shader, Render::JGraphicResource& outResource) const;

private:
	JMaterialResource& GetOrCreateMaterialResource(uint32 materialID);
	uint32 FindMaterialResourceIndex(uint32 materialID) const;

	std::vector<MaterialResourceRecord> _materialResources;
	std::unordered_map<uint32, uint32> _materialIndexMap;
};

J_ENGINE_END

#endif
