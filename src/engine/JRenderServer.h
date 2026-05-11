#pragma once
#ifndef __J_RENDER_SERVER_H__
#define __J_RENDER_SERVER_H__

#include "engine/precompile.h"
#include "engine/JRenderDB.h"

/*#include "engine/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }
/*#include "engine/JGraphicResource.h"*/ namespace J { namespace Render { class JGraphicResource; class JShader; } }

J_ENGINE_BEGIN

class JRenderServer
{
public:
	struct MaterialRecord
	{
		uint32 materialID = 0;
		JMaterial* source = nullptr;
	};

	JRenderServer() = default;

	JRenderDB& GetRenderDB() { return _renderDB; }
	const JRenderDB& GetRenderDB() const { return _renderDB; }

	void RegisterMaterial(JMaterial* material);
	void UnregisterMaterial(uint32 materialID);
	void MarkMaterialDirty(JMaterial* material);
	void Sync();

	bool BuildGraphicResource(uint32 materialID, Render::JShader* shader, Render::JGraphicResource& outResource) const;

private:
	uint32 FindMaterialIndex(uint32 materialID) const;
	JMaterial* FindMaterial(uint32 materialID) const;

	JRenderDB _renderDB;
	std::vector<MaterialRecord> _materials;
	std::unordered_map<uint32, uint32> _materialIndexMap;
	std::vector<uint32> _dirtyMaterialIDs;
};

J_ENGINE_END

#endif
