#include "client/editor/JScenePanel.h"

#include "engine/render/JSwapChain.h"
#include "engine/render/JRenderDefinition.h"
#include "engine/render/JRenderDB.h"
#include "engine/render/JRenderServer.h"
#include "engine/render/JRenderer.h"
#include "engine/render/JMaterialFactory.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"

#include <iostream>
#include <algorithm>

J_EDITOR_BEGIN

namespace
{
	constexpr float MOUSE_LOOK_SENSITIVITY = 0.0025f;
	constexpr float MAX_FRAME_DELTA_TIME = 0.1f;
	constexpr float CAMERA_SPEED_MIN = 0.5f;
	constexpr float CAMERA_SPEED_MAX = 100.0f;
	constexpr float CAMERA_SPEED_STEP_MULTIPLIER = 1.2f;
	constexpr float CAMERA_SPEED_SHIFT_MULTIPLIER = 4.0f;
	constexpr float EDITOR_GRID_SIZE = 5000.0f;
	constexpr float EDITOR_GRID_Y = -0.1f;

	XMVECTOR getForward(float yaw, float pitch)
	{
		const float cosPitch = cosf(pitch);
		return XMVector3Normalize(XMVectorSet(
			sinf(yaw) * cosPitch,
			sinf(pitch),
			cosf(yaw) * cosPitch,
			0.0f));
	}

	XMVECTOR getForwardXZ(float yaw)
	{
		return XMVector3Normalize(XMVectorSet(sinf(yaw), 0.0f, cosf(yaw), 0.0f));
	}

	XMVECTOR getRight(float yaw)
	{
		const XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
		return XMVector3Normalize(XMVector3Cross(worldUp, getForwardXZ(yaw)));
	}

	XMVECTOR getUp(float yaw, float pitch)
	{
		const XMVECTOR forward = getForward(yaw, pitch);
		const XMVECTOR right = getRight(yaw);
		return XMVector3Normalize(XMVector3Cross(forward, right));
	}

	uint32 getClientWidth(HWND hwnd)
	{
		RECT rect{};
		if (hwnd == nullptr || !GetClientRect(hwnd, &rect))
		{
			return 1920u;
		}
		return static_cast<uint32>(std::max<LONG>(rect.right - rect.left, 1));
	}

	uint32 getClientHeight(HWND hwnd)
	{
		RECT rect{};
	if (hwnd == nullptr || !GetClientRect(hwnd, &rect))
	{
		return 1080u;
	}
	return static_cast<uint32>(std::max<LONG>(rect.bottom - rect.top, 1));
	}
}

JScenePanel::JScenePanel(JSceneManager* sceneManager)
	: _sceneManager(sceneManager)
{
}

JScenePanel::~JScenePanel()
{
	destroyStatsPopup();

	if (_editorGridMaterial != nullptr)
	{
		if (Engine::JRenderServer* renderServer = GetEngine() != nullptr ? GetEngine()->GetRenderServer() : nullptr)
		{
			renderServer->UnregisterMaterial(_editorGridMaterial->instanceID);
		}
	}
}

Engine::JScene* JScenePanel::getScene()
{
	return _sceneManager != nullptr ? _sceneManager->GetScene() : nullptr;
}

const Engine::JScene* JScenePanel::getScene() const
{
	return _sceneManager != nullptr ? _sceneManager->GetScene() : nullptr;
}

void JScenePanel::Init()
{
	if (_mainWindow == nullptr)
	{
		_mainWindow = GetActiveWindow();
		if (_mainWindow == nullptr)
		{
			_mainWindow = GetForegroundWindow();
		}
	}

	if (_sceneManager == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: scene manager is null." << std::endl;
		return;
	}

	const uint32 initialWidth = getClientWidth(_mainWindow);
	const uint32 initialHeight = getClientHeight(_mainWindow);
	_viewportWidth = initialWidth;
	_viewportHeight = initialHeight;

	Engine::JScene* scene = getScene();
	_sceneCamera = _sceneManager->GetPrimaryCamera();
	_light = _sceneManager->GetFirstLight();
	if (scene == nullptr || !_sceneCamera.IsValid())
	{
		std::cerr << "JScenePanel::Init failed: scene or camera is invalid." << std::endl;
		return;
	}

	if (scene->GetCamera(_sceneCamera) == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: camera data access failed." << std::endl;
		return;
	}

	createEditorGrid();
	selectDefaultRenderObject();
	createStatsPopup();

	Engine::JRenderer* renderer = GetEngine()->GetRenderer();
	if (renderer != nullptr)
	{
		renderer->SetRenderPath(Engine::JRenderer::RenderPath::Deferred);
	}

	QueryPerformanceFrequency(&_timerFrequency);
	QueryPerformanceCounter(&_lastFrameCounter);
	_isReady = true;
}

void JScenePanel::Update()
{
	if (!_isReady)
	{
		return;
	}

	Render::JSwapChain* swapChain = GetEngine()->GetSwapChain();
	Engine::JRenderServer* renderServer = GetEngine()->GetRenderServer();
	Engine::JRenderer* renderer = GetEngine()->GetRenderer();
	Engine::JScene* scene = getScene();
	if (swapChain == nullptr || renderServer == nullptr || renderer == nullptr || scene == nullptr || !_sceneCamera.IsValid())
	{
		std::cerr << "JScenePanel::Update skipped: render resources are not ready." << std::endl;
		_isReady = false;
		return;
	}

	const uint32 clientWidth = getClientWidth(_mainWindow);
	const uint32 clientHeight = getClientHeight(_mainWindow);
	if (clientWidth != _viewportWidth || clientHeight != _viewportHeight)
	{
		_viewportWidth = clientWidth;
		_viewportHeight = clientHeight;
		if (swapChain != nullptr)
		{
			if (Render::JCommandQueue* commandQueue = GetEngine()->GetCmdQueue())
			{
				commandQueue->WaitIdle();
			}
			swapChain->Resize(clientWidth, clientHeight);
		}

		Engine::JScene::CameraData* cameraData = scene->GetCamera(_sceneCamera);
		if (cameraData != nullptr)
		{
			scene->SetCameraAspectRatio(_sceneCamera, static_cast<float>(clientWidth) / static_cast<float>(clientHeight));
		}
	}

	const float deltaTime = tickFrameTimer();

	updateSceneCamera(deltaTime);
	updateSelectedObject(deltaTime);
	if (_mainWindow != nullptr && GetForegroundWindow() == _mainWindow && (GetAsyncKeyState(VK_F1) & 0x0001))
	{
		_showStatsPopup = !_showStatsPopup;
		if (_statsPopup != nullptr)
		{
			ShowWindow(_statsPopup, _showStatsPopup ? SW_SHOWNOACTIVATE : SW_HIDE);
		}
	}

	renderServer->MarkCameraDirty(_sceneCamera);
	renderServer->Sync();
	renderServer->SyncScene(*scene);

	Render::JViewport viewport = { 0, 0, static_cast<float>(clientWidth), static_cast<float>(clientHeight), 0, 1 };
	D3D12_RECT rect = CD3DX12_RECT(0, 0, static_cast<LONG>(clientWidth), static_cast<LONG>(clientHeight));

	Engine::JRenderer::FrameDesc frameDesc;
	if (!renderServer->BuildFrameDesc(swapChain->GetRenderTarget(), JColors::DarkGray, viewport, rect, frameDesc))
	{
		std::cerr << "JScenePanel::Update skipped: failed to build frame description." << std::endl;
		return;
	}

	updateStatsPopup(frameDesc, deltaTime);
	renderer->Render(frameDesc);
}

float JScenePanel::tickFrameTimer()
{
	if (_timerFrequency.QuadPart == 0 || _lastFrameCounter.QuadPart == 0)
	{
		QueryPerformanceFrequency(&_timerFrequency);
		QueryPerformanceCounter(&_lastFrameCounter);
		return 0.0f;
	}

	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);
	const double elapsed = static_cast<double>(now.QuadPart - _lastFrameCounter.QuadPart) / static_cast<double>(_timerFrequency.QuadPart);
	_lastFrameCounter = now;
	return std::clamp(static_cast<float>(elapsed), 0.0f, MAX_FRAME_DELTA_TIME);
}

void JScenePanel::OnMouseWheel(short delta)
{
	if (!_isReady || _mainWindow == nullptr || GetForegroundWindow() != _mainWindow)
	{
		return;
	}

	if (delta > 0)
	{
		_editorCameraMoveSpeed *= CAMERA_SPEED_STEP_MULTIPLIER;
	}
	else if (delta < 0)
	{
		_editorCameraMoveSpeed /= CAMERA_SPEED_STEP_MULTIPLIER;
	}

	_editorCameraMoveSpeed = std::clamp(_editorCameraMoveSpeed, CAMERA_SPEED_MIN, CAMERA_SPEED_MAX);

	Engine::JScene* scene = getScene();
	if (scene == nullptr || !_sceneCamera.IsValid())
	{
		return;
	}
}

void JScenePanel::createEditorGrid()
{
	Engine::JScene* scene = getScene();
	Engine::JEngine* engine = GetEngine();
	if (scene == nullptr || engine == nullptr)
	{
		return;
	}

	if (_editorGridRenderObject.IsValid())
	{
		return;
	}

	Engine::JMaterialFactory* materialFactory = engine->GetMaterialFactory();
	Engine::JRenderServer* renderServer = engine->GetRenderServer();
	if (materialFactory == nullptr || renderServer == nullptr)
	{
		return;
	}

	const std::string gridShaderPath = (std::filesystem::path(get_Engine_Res_Path()) / "shader" / "grid.hlsl").string();
	_editorGridMaterial.reset(materialFactory->CreateMaterial(gridShaderPath, true));
	if (_editorGridMaterial == nullptr)
	{
		std::cerr << "JScenePanel::createEditorGrid failed: grid material creation failed." << std::endl;
		return;
	}

	_editorGridMesh = std::make_unique<Engine::JMesh>();
	_editorGridMesh->SetPositions({
		-EDITOR_GRID_SIZE, EDITOR_GRID_Y, -EDITOR_GRID_SIZE, 1.0f,
		 EDITOR_GRID_SIZE, EDITOR_GRID_Y, -EDITOR_GRID_SIZE, 1.0f,
		 EDITOR_GRID_SIZE, EDITOR_GRID_Y,  EDITOR_GRID_SIZE, 1.0f,
		-EDITOR_GRID_SIZE, EDITOR_GRID_Y,  EDITOR_GRID_SIZE, 1.0f,
	});
	_editorGridMesh->SetIndices({ 0, 1, 2, 0, 2, 3 });

	_editorGridEntity = scene->CreateEntity("__editor_grid", "Editor Grid", { "editor_only" });
	if (!_editorGridEntity.IsValid())
	{
		return;
	}

	Engine::JScene::TransformData transform{};
	transform.translation = { 0.0f, 0.0f, 0.0f };
	transform.rotation = { 0.0f, 0.0f, 0.0f };
	transform.scale = { 1.0f, 1.0f, 1.0f };
	scene->AddTransform(_editorGridEntity, transform);

	renderServer->RegisterMaterial(_editorGridMaterial.get());
	_editorGridRenderObject = scene->AddRenderObjectComponent(
		_editorGridEntity,
		_editorGridMaterial->instanceID,
		_editorGridMesh.get(),
		true);
}

void JScenePanel::updateSceneCamera(float deltaTime)
{
	Engine::JScene* scene = getScene();
	if (scene == nullptr || !_sceneCamera.IsValid())
	{
		return;
	}

	if (_mainWindow == nullptr || GetForegroundWindow() != _mainWindow)
	{
		_isMouseLookActive = false;
		return;
	}

	Engine::JScene::CameraData* cameraData = scene->GetCamera(_sceneCamera);
	if (cameraData == nullptr)
	{
		return;
	}

	const Engine::JTransformHandle transformHandle = scene->GetTransformHandle(cameraData->entity);
	if (!transformHandle.IsValid())
	{
		return;
	}

	const Engine::JScene::TransformData transformData = scene->GetTransform(transformHandle);

	float yawInput = 0.0f;
	float pitchInput = 0.0f;
	float forwardInput = 0.0f;
	float rightInput = 0.0f;
	float upInput = 0.0f;
	float moveSpeedMultiplier = 1.0f;

	if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)
	{
		moveSpeedMultiplier = CAMERA_SPEED_SHIFT_MULTIPLIER;
	}

	if (GetAsyncKeyState(VK_LEFT) & 0x8000) yawInput -= deltaTime;
	if (GetAsyncKeyState(VK_RIGHT) & 0x8000) yawInput += deltaTime;
	if (GetAsyncKeyState(VK_UP) & 0x8000) pitchInput += deltaTime;
	if (GetAsyncKeyState(VK_DOWN) & 0x8000) pitchInput -= deltaTime;

	if (GetAsyncKeyState('W') & 0x8000) forwardInput += 1.0f;
	if (GetAsyncKeyState('S') & 0x8000) forwardInput -= 1.0f;
	if (GetAsyncKeyState('D') & 0x8000) rightInput += 1.0f;
	if (GetAsyncKeyState('A') & 0x8000) rightInput -= 1.0f;
	if (GetAsyncKeyState('E') & 0x8000) upInput += 1.0f;
	if (GetAsyncKeyState('Q') & 0x8000) upInput -= 1.0f;

	const bool isRightMouseDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	if (isRightMouseDown)
	{
		POINT currentMousePosition{};
		if (GetCursorPos(&currentMousePosition))
		{
			if (_isMouseLookActive)
			{
				const LONG deltaX = currentMousePosition.x - _lastMousePosition.x;
				const LONG deltaY = currentMousePosition.y - _lastMousePosition.y;
				yawInput += static_cast<float>(deltaX) * MOUSE_LOOK_SENSITIVITY;
				pitchInput -= static_cast<float>(deltaY) * MOUSE_LOOK_SENSITIVITY;
			}

			_lastMousePosition = currentMousePosition;
			_isMouseLookActive = true;
		}
	}
	else
	{
		_isMouseLookActive = false;
	}

	const float moveSpeed = _editorCameraMoveSpeed * moveSpeedMultiplier;
	XMFLOAT3 newRotation = {
		transformData.rotation.x,
		transformData.rotation.y,
		transformData.rotation.z
	};
	newRotation.y += yawInput;
	newRotation.x += pitchInput;

	const XMVECTOR forwardVector = getForward(newRotation.y, newRotation.x);
	const XMVECTOR rightVector = getRight(newRotation.y);
	const XMVECTOR upVector = getUp(newRotation.y, newRotation.x);
	XMVECTOR position = XMVectorSet(transformData.translation.x, transformData.translation.y, transformData.translation.z, 1.0f);
	position += forwardVector * (forwardInput * moveSpeed * deltaTime);
	position += rightVector * (rightInput * moveSpeed * deltaTime);
	position += upVector * (upInput * moveSpeed * deltaTime);

	XMFLOAT3 newPosition;
	XMStoreFloat3(&newPosition, position);

	scene->SetTransformRotation(transformHandle, { newRotation.x, newRotation.y, newRotation.z });
	scene->SetTransformTranslation(transformHandle, { newPosition.x, newPosition.y, newPosition.z });
}

void JScenePanel::selectDefaultRenderObject()
{
	Engine::JScene* scene = getScene();
	if (scene == nullptr)
	{
		return;
	}

	const std::vector<Engine::JScene::RenderObjectComponentSlot>& slots = scene->GetRenderObjectComponentSlots();
	for (const Engine::JScene::RenderObjectComponentSlot& slot : slots)
	{
		if (!slot.active || !slot.data.active || !slot.data.visible || slot.data.transparent)
		{
			continue;
		}

		_selectedRenderObject = { static_cast<uint32>(&slot - slots.data()), slot.generation };
		_selectedEntity = slot.data.entity;
		return;
	}
}

void JScenePanel::updateSelectedObject(float deltaTime)
{
	(void)deltaTime;

	Engine::JScene* scene = getScene();
	if (scene == nullptr || !_selectedEntity.IsValid() || _mainWindow == nullptr || GetForegroundWindow() != _mainWindow)
	{
		return;
	}

	if (GetAsyncKeyState('V') & 0x0001)
	{
		Engine::JScene::RenderObjectComponentData* renderObject = scene->GetRenderObjectComponent(_selectedEntity);
		if (renderObject != nullptr)
		{
			scene->SetRenderObjectVisible(_selectedEntity, !renderObject->visible);
		}
	}
}

void JScenePanel::createStatsPopup()
{
	if (_statsPopup != nullptr || _mainWindow == nullptr)
	{
		return;
	}

	RECT ownerRect{};
	GetWindowRect(_mainWindow, &ownerRect);
	_statsPopup = CreateWindowExW(
		WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
		L"STATIC",
		L"FPS: 0.0\r\nDrawCalls: 0",
		WS_POPUP | WS_BORDER | SS_LEFT,
		ownerRect.left + 20,
		ownerRect.top + 54,
		260,
		96,
		_mainWindow,
		nullptr,
		GetModuleHandleW(nullptr),
		nullptr);

	if (_statsPopup == nullptr)
	{
		return;
	}

	_statsFont = CreateFontW(
		24,
		0,
		0,
		0,
		FW_SEMIBOLD,
		FALSE,
		FALSE,
		FALSE,
		DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE,
		L"Consolas");
	SendMessageW(_statsPopup, WM_SETFONT, reinterpret_cast<WPARAM>(_statsFont), TRUE);
	ShowWindow(_statsPopup, _showStatsPopup ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void JScenePanel::destroyStatsPopup()
{
	if (_statsPopup != nullptr)
	{
		DestroyWindow(_statsPopup);
		_statsPopup = nullptr;
	}

	if (_statsFont != nullptr)
	{
		DeleteObject(_statsFont);
		_statsFont = nullptr;
	}
}

void JScenePanel::updateStatsPopup(const Engine::JFrameDesc& frameDesc, float rawDeltaTime)
{
	constexpr float STATS_UPDATE_INTERVAL = 0.25f;
	_statsElapsed += rawDeltaTime;
	++_statsFrameCount;

	if (_statsPopup == nullptr)
	{
		createStatsPopup();
	}

	if (_statsPopup == nullptr || !_showStatsPopup)
	{
		return;
	}

	RECT ownerRect{};
	if (_mainWindow != nullptr && GetWindowRect(_mainWindow, &ownerRect))
	{
		SetWindowPos(_statsPopup, HWND_TOPMOST, ownerRect.left + 20, ownerRect.top + 54, 260, 96, SWP_NOACTIVATE);
	}

	if (_statsElapsed >= STATS_UPDATE_INTERVAL)
	{
		_displayFps = _statsElapsed > 0.0f ? static_cast<float>(_statsFrameCount) / _statsElapsed : 0.0f;
		_displayFrameMs = _displayFps > 0.0f ? 1000.0f / _displayFps : 0.0f;
		_displayDrawCallCount = static_cast<uint32>(frameDesc.opaqueDrawItemIndices.size() + frameDesc.transparentDrawItemIndices.size());
		_statsElapsed = 0.0f;
		_statsFrameCount = 0;
	}

	wchar_t text[128] = {};
	swprintf_s(text, L"FPS: %.1f\r\nFrame: %.2f ms\r\nDrawCalls: %u", _displayFps, _displayFrameMs, _displayDrawCallCount);
	SetWindowTextW(_statsPopup, text);
}

J_EDITOR_END
