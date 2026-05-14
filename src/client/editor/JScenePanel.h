#pragma once
#ifndef __J_SCENE_PANEL_H__
#define __J_SCENE_PANEL_H__

#include "client/editor/JEditorPanel.h"
#include "client/editor/JSceneManager.h"
#include "engine/core/JEngineContext.h"
#include "engine/scene/JScene.h"

#include <chrono>

J_EDITOR_BEGIN

class JScenePanel : public JEditorPanel
{
public:
	explicit JScenePanel(JSceneManager* sceneManager);
	~JScenePanel() override;

	void Init() override;
	void Update() override;
	void OnMouseWheel(short delta) override;

private:
	Engine::JScene* getScene();
	const Engine::JScene* getScene() const;
	void updateSceneCamera(float deltaTime);
	
	JSceneManager* _sceneManager = nullptr;
	Engine::JCameraHandle _sceneCamera = {};
	Engine::JLightHandle _light = {};
	bool _isMouseLookActive = false;
	POINT _lastMousePosition = {};
	bool _isReady = false;
	std::chrono::steady_clock::time_point _lastUpdateTime = {};
	float _editorCameraMoveSpeed = 6.0f;
	HWND _cameraInfoWindow = nullptr;
	HWND _cameraInfoText = nullptr;
	HWND _mainWindow = nullptr;
	uint32 _viewportWidth = 0;
	uint32 _viewportHeight = 0;

	void updateCameraInfoPanel();
	void createCameraInfoPanel();

};

J_EDITOR_END

#endif
