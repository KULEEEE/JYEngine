#pragma once
#ifndef __J_SCENE_PANEL_H__
#define __J_SCENE_PANEL_H__

#include "client/editor/JEditorPanel.h"
#include "Engine/JEngineContext.h"
#include "Engine/JCommandQueue.h"
#include "engine/JScene.h"

/*#include "engine/asset/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }
/*#include "engine/asset/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }
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
	void updateCameraInfoPanel();
	void createCameraInfoPanel();

	Render::JCommandQueue* _commandQueue = nullptr;
	Engine::JScene* scene = nullptr;
	Engine::JMaterial* material = nullptr;
	Engine::JMaterial* planeMaterial = nullptr;
	Engine::JMesh* mesh = nullptr;
	Engine::JMesh* planeMesh = nullptr;
	Render::JConstantBuffer* perFrameBuffer = nullptr;
	Render::JConstantBuffer* materialBuffer = nullptr;
	Render::JTexture* materialTexture = nullptr;
	Engine::JEntityHandle _cameraEntity = {};
	Engine::JEntityHandle _lightEntity = {};
	Engine::JEntityHandle _planeEntity = {};
	Engine::JEntityHandle _carEntity = {};
	Engine::JTransformHandle _cameraTransform = {};
	Engine::JTransformHandle _lightTransform = {};
	Engine::JTransformHandle _planeTransform = {};
	Engine::JTransformHandle _carTransform = {};
	Engine::JCameraHandle _camera = {};
	Engine::JLightHandle _light = {};
	Engine::JRenderObjectHandle _planeRenderObject = {};
	Engine::JRenderObjectHandle _carRenderObject = {};
	bool _isMouseLookActive = false;
	POINT _lastMousePosition = {};
	bool _isReady = false;
	HWND _cameraInfoWindow = nullptr;
	HWND _cameraInfoText = nullptr;
	HWND _mainWindow = nullptr;
};

J_EDITOR_END

#endif
