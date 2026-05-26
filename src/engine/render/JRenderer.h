#pragma once
#ifndef __J_RENDERER_H__
#define __J_RENDERER_H__

#include "engine/precompile.h"
#include "engine/render/JGBuffer.h"
#include "engine/render/JRenderDB.h"
#include "engine/render/JRenderFrame.h"
#include "engine/render/JRenderPass.h"

/*#include "engine/render/JCommandQueue.h"*/ namespace J { namespace Render { class JCommandQueue; } }
/*#include "engine/render/JRenderContext.h"*/ namespace J { namespace Render { class JRenderContext; } }
/*#include "engine/asset/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }

J_ENGINE_BEGIN

class JRenderer
{
public:
	enum class RenderPath
	{
		Forward,
		Deferred
	};

	using DrawItem = JDrawItem;
	using FrameDesc = JFrameDesc;

	JRenderer() = default;
	~JRenderer();

	void Initialize(Render::JCommandQueue* commandQueue, Render::JRenderContext* renderContext);
	JRenderDB& GetRenderDB() { return _renderDB; }
	const JRenderDB& GetRenderDB() const { return _renderDB; }
	void RegisterMaterial(JMaterial* material);
	void UnregisterMaterial(uint32 materialID);
	void MarkMaterialDirty(JMaterial* material);
	void SetRenderPath(RenderPath renderPath);
	RenderPath GetRenderPath() const { return _renderPath; }
	JGBuffer* GetGBuffer() const { return _gBuffer.get(); }

	void Render(const FrameDesc& frameDesc);

private:
	void initializeDefaultPasses();
	void initializeForwardPasses();
	void initializeDeferredPasses();
	void ensureGBuffer(const FrameDesc& frameDesc);
	void syncMaterials();
	void prepareFrameResources(const FrameDesc& frameDesc);
	uint32 findMaterialIndex(uint32 materialID) const;
	JMaterial* findMaterial(uint32 materialID) const;

	struct MaterialRecord
	{
		uint32 materialID = 0;
		JMaterial* source = nullptr;
	};

	Render::JCommandQueue* _commandQueue = nullptr;
	Render::JRenderContext* _renderContext = nullptr;
	JRenderDB _renderDB;
	std::vector<MaterialRecord> _materials;
	std::unordered_map<uint32, uint32> _materialIndexMap;
	std::vector<uint32> _dirtyMaterialIDs;
	std::vector<std::unique_ptr<JRenderPass>> _passes;
	std::unique_ptr<JGBuffer> _gBuffer;
	RenderPath _renderPath = RenderPath::Forward;
};

J_ENGINE_END

#endif
