#pragma once
#ifndef __J_SCENE_PANEL_H__
#define __J_SCENE_PANEL_H__

#include "client/editor/JEditorPanel.h"
#include "engine/scene/JScene.h"

#include <memory>

namespace J::Engine
{
	class JMaterial;
	class JMesh;
	class JRenderTarget;
	struct JFrameDesc;
}

J_EDITOR_BEGIN

class JScenePanel : public JEditorPanel
{
public:
	JScenePanel() = default;
	~JScenePanel() override;

	void Init() override;
	void Update() override;
	void Update(Engine::JScene& scene);
	void OnMouseWheel(short delta) override;
	void SetRenderTarget(Engine::JRenderTarget* renderTarget) { _renderTarget = renderTarget; }
	Engine::JRenderTarget* GetRenderTarget() const { return _renderTarget; }
	bool CanRender() const;
#ifdef _DEBUG
	void OnSceneRendered(const Engine::JFrameDesc& frameDesc);
#endif

private:
	void createEditorGrid(Engine::JScene& scene);
	void updateSceneCamera(Engine::JScene& scene, Engine::JCameraHandle sceneCamera, float deltaTime);
	void selectDefaultRenderObject(Engine::JScene& scene);
	void updateSelectedObject(Engine::JScene& scene, float deltaTime);
#ifdef _DEBUG
	void createStatsPopup();
	void destroyStatsPopup();
	void updateStatsPopup(const Engine::JFrameDesc& frameDesc);
#endif
	float tickFrameTimer();
	
	Engine::JEntityHandle _selectedEntity = {};
	std::unique_ptr<Engine::JMaterial> _editorGridMaterial;
	std::unique_ptr<Engine::JMesh> _editorGridMesh;
	bool _isMouseLookActive = false;
#ifdef _DEBUG
	bool _showStatsPopup = true;
#endif
	POINT _lastMousePosition = {};
	bool _isReady = false;
	LARGE_INTEGER _timerFrequency = {};
	LARGE_INTEGER _lastFrameCounter = {};
	float _editorCameraMoveSpeed = 6.0f;
	HWND _mainWindow = nullptr;
	uint32 _viewportWidth = 0;
	uint32 _viewportHeight = 0;
#ifdef _DEBUG
	HWND _statsPopup = nullptr;
	HFONT _statsFont = nullptr;
	float _statsElapsed = 0.0f;
	uint32 _statsFrameCount = 0;
	float _displayFps = 0.0f;
	float _displayFrameMs = 0.0f;
	uint32 _displayDrawCallCount = 0;
	float _lastDeltaTime = 0.0f;
#endif
	Engine::JRenderTarget* _renderTarget = nullptr;

};

J_EDITOR_END

#endif
