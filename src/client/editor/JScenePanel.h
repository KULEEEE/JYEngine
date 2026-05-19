#pragma once
#ifndef __J_SCENE_PANEL_H__
#define __J_SCENE_PANEL_H__

#include "client/editor/JEditorPanel.h"
#include "client/editor/JSceneManager.h"
#include "engine/core/JEngineContext.h"
#include "engine/scene/JScene.h"

#include <chrono>
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
	void populateDebugOverlay(Engine::JFrameDesc& frameDesc, float deltaTime);
	
	JSceneManager* _sceneManager = nullptr;
	Engine::JCameraHandle _sceneCamera = {};
	Engine::JLightHandle _light = {};
	Engine::JRenderObjectHandle _selectedRenderObject = {};
	Engine::JEntityHandle _selectedEntity = {};
	Engine::JEntityHandle _editorGridEntity = {};
	Engine::JRenderObjectHandle _editorGridRenderObject = {};
	std::unique_ptr<Engine::JMaterial> _editorGridMaterial;
	std::unique_ptr<Engine::JMesh> _editorGridMesh;
	bool _isMouseLookActive = false;
	POINT _lastMousePosition = {};
	bool _isReady = false;
	std::chrono::steady_clock::time_point _lastUpdateTime = {};
	float _editorCameraMoveSpeed = 6.0f;
	HWND _mainWindow = nullptr;
	uint32 _viewportWidth = 0;
	uint32 _viewportHeight = 0;

};

J_EDITOR_END

#endif
