#pragma once
#ifndef __J_SCENE_PANEL_H__
#define __J_SCENE_PANEL_H__

#include "client/editor/JEditorPanel.h"
#include "Engine/JEngineContext.h"
#include "Engine/JCommandQueue.h"

/*#include "engine/asset/JShader.h"*/ namespace J { namespace Render { class JShader; } }
/*#include "engine/asset/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }
/*#include "engine/asset/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }
/*#include "engine/JCameraComponent.h"*/ namespace J { namespace Engine { class JCameraComponent; } }
/*#include "engine/JGraphicResource.h"*/ namespace J { namespace Render { class JGraphicResource; } }
/*#include "engine/JRenderDefinition.h"*/ namespace J { namespace Render { struct JPipeline; } }
/*#include "engine/JRenderDefinition.h"*/ namespace J { namespace Render { struct JVertexBuffer; } }
/*#include "engine/JRenderDefinition.h"*/ namespace J { namespace Render { struct JIndexBuffer; } }
/*#include "engine/JRenderDefinition.h"*/ namespace J { namespace Render { struct JConstantBuffer; } }
/*#include "engine/JRenderDefinition.h"*/ namespace J { namespace Render { struct JTexture; } }

J_EDITOR_BEGIN

class JScenePanel : public JEditorPanel
{
public:
	JScenePanel() = default;
	~JScenePanel() override;

	void Init() override;
	void Update() override;

private:
	void updateCamera(float deltaTime);
	void updatePerFrameBuffer();
	void updateCameraInfoPanel();
	void createCameraInfoPanel();

	Render::JCommandQueue* _commandQueue = nullptr;
	Render::JShader* shader = nullptr;
	Render::JPipeline* pipeline = nullptr;
	Render::JShader* planeShader = nullptr;
	Render::JPipeline* planePipeline = nullptr;
	Engine::JMaterial* material = nullptr;
	Render::JGraphicResource* graphicResource = nullptr;
	Engine::JMaterial* planeMaterial = nullptr;
	Render::JGraphicResource* planeGraphicResource = nullptr;
	Engine::JMesh* mesh = nullptr;
	Engine::JMesh* planeMesh = nullptr;
	Render::JVertexBuffer* vertexBuffer = nullptr;
	Render::JIndexBuffer* indexBuffer = nullptr;
	Render::JVertexBuffer* planeVertexBuffer = nullptr;
	Render::JIndexBuffer* planeIndexBuffer = nullptr;
	Render::JConstantBuffer* perFrameBuffer = nullptr;
	Render::JConstantBuffer* materialBuffer = nullptr;
	Render::JTexture* materialTexture = nullptr;
	Engine::JCameraComponent* camera = nullptr;
	bool _isMouseLookActive = false;
	POINT _lastMousePosition = {};
	bool _isReady = false;
	HWND _cameraInfoWindow = nullptr;
	HWND _cameraInfoText = nullptr;
	HWND _mainWindow = nullptr;
};

J_EDITOR_END

#endif
