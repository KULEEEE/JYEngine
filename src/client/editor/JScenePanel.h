#pragma once
#ifndef __J_SCENE_PANEL_H__
#define __J_SCENE_PANEL_H__

#include "client/editor/JEditorPanel.h"
#include "client/editor/JSceneManager.h"
#include "engine/core/JEngineContext.h"
#include "engine/scene/JScene.h"

#include <memory>

namespace J::Engine
{
	class JMaterial;
	class JMesh;
	struct JFrameDesc;
}

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
	void createEditorGrid();
	void updateSceneCamera(float deltaTime);
	void selectDefaultRenderObject();
	void updateSelectedObject(float deltaTime);
	void createStatsPopup();
	void destroyStatsPopup();
	void updateStatsPopup(const Engine::JFrameDesc& frameDesc, float rawDeltaTime);
	float tickFrameTimer();
	
	JSceneManager* _sceneManager = nullptr;
	Engine::JCameraHandle _sceneCamera = {};
	Engine::JLightHandle _light = {};
	Engine::JRenderObjectComponentHandle _selectedRenderObject = {};
	Engine::JEntityHandle _selectedEntity = {};
	Engine::JEntityHandle _editorGridEntity = {};
	Engine::JRenderObjectComponentHandle _editorGridRenderObject = {};
	std::unique_ptr<Engine::JMaterial> _editorGridMaterial;
	std::unique_ptr<Engine::JMesh> _editorGridMesh;
	bool _isMouseLookActive = false;
	bool _showStatsPopup = true;
	POINT _lastMousePosition = {};
	bool _isReady = false;
	LARGE_INTEGER _timerFrequency = {};
	LARGE_INTEGER _lastFrameCounter = {};
	float _editorCameraMoveSpeed = 6.0f;
	HWND _mainWindow = nullptr;
	uint32 _viewportWidth = 0;
	uint32 _viewportHeight = 0;
	HWND _statsPopup = nullptr;
	HFONT _statsFont = nullptr;
	float _statsElapsed = 0.0f;
	uint32 _statsFrameCount = 0;
	float _displayFps = 0.0f;
	float _displayFrameMs = 0.0f;
	uint32 _displayDrawCallCount = 0;

};

J_EDITOR_END

#endif
