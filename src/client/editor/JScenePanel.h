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
	void DrawEditorUI(Engine::JScene& scene);
#ifdef _DEBUG
	void DrawStatsUI();
#endif
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
#ifdef _DEBUG
	void updateStatsData(const Engine::JFrameDesc& frameDesc);
#endif
	float tickFrameTimer();
	
	// Grid Pass Asset
	std::shared_ptr<Engine::JMaterial> _editorGridMaterial;
	Engine::JMaterialHandle _editorGridMaterialHandle = {};
	std::unique_ptr<Engine::JMesh> _editorGridMesh;

	// editor Camera Parameter
	bool _isMouseLookActive = false;
	POINT _lastMousePosition = {};
	float _editorCameraMoveSpeed = 6.0f;

	LARGE_INTEGER _timerFrequency = {};
	LARGE_INTEGER _lastFrameCounter = {};

	Engine::JRenderTarget* _renderTarget = nullptr;

	bool _showEditorUI = true;
	float _lastDeltaTime = 0.0f;

#ifdef _DEBUG
	bool _showStatsPopup = true;
	float _statsElapsed = 0.0f;
	uint32 _statsFrameCount = 0;
	float _displayFps = 0.0f;
	float _displayFrameMs = 0.0f;
	uint32 _displayDrawCallCount = 0;
#endif
	

};

J_EDITOR_END

#endif
