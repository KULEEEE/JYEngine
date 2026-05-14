#include "client/editor/JScenePanel.h"

#include "engine/render/JSwapChain.h"
#include "engine/render/JRenderDefinition.h"
#include "engine/render/JRenderDB.h"
#include "engine/render/JRenderServer.h"
#include "engine/render/JRenderer.h"
#include "engine/render/JMaterialFactory.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>

J_EDITOR_BEGIN

namespace
{
	constexpr float MOUSE_LOOK_SENSITIVITY = 0.0025f;
	constexpr float MAX_FRAME_DELTA_TIME = 0.1f;
	constexpr float CAMERA_SPEED_MIN = 0.5f;
	constexpr float CAMERA_SPEED_MAX = 100.0f;
	constexpr float CAMERA_SPEED_STEP_MULTIPLIER = 1.2f;
	constexpr float CAMERA_SPEED_SHIFT_MULTIPLIER = 4.0f;

	float toDegrees(float radians)
	{
		return XMConvertToDegrees(radians);
	}

	float wrapDegrees(float degrees)
	{
		while (degrees > 180.0f)
		{
			degrees -= 360.0f;
		}

		while (degrees < -180.0f)
		{
			degrees += 360.0f;
		}

		return degrees;
	}

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
	if (_cameraInfoWindow != nullptr)
	{
		DestroyWindow(_cameraInfoWindow);
		_cameraInfoWindow = nullptr;
		_cameraInfoText = nullptr;
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
	_mainWindow = GetActiveWindow();
	if (_mainWindow == nullptr)
	{
		_mainWindow = GetForegroundWindow();
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

	Engine::JRenderer* renderer = GetEngine()->GetRenderer();
	if (renderer != nullptr)
	{
		renderer->SetRenderPath(Engine::JRenderer::RenderPath::Deferred);
	}

	createCameraInfoPanel();
	updateCameraInfoPanel();
	_lastUpdateTime = std::chrono::steady_clock::now();
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
			swapChain->Resize(clientWidth, clientHeight);
		}

		Engine::JScene::CameraData* cameraData = scene->GetCamera(_sceneCamera);
		if (cameraData != nullptr)
		{
			cameraData->aspectRatio = static_cast<float>(clientWidth) / static_cast<float>(clientHeight);
		}
	}

	const auto now = std::chrono::steady_clock::now();
	float deltaTime = 0.0f;
	if (_lastUpdateTime != std::chrono::steady_clock::time_point{})
	{
		deltaTime = std::chrono::duration<float>(now - _lastUpdateTime).count();
	}
	_lastUpdateTime = now;
	deltaTime = std::clamp(deltaTime, 0.0f, MAX_FRAME_DELTA_TIME);

	updateSceneCamera(deltaTime);
	renderServer->MarkCameraDirty(_sceneCamera);
	renderServer->Sync();
	renderServer->SyncScene(*scene);
	updateCameraInfoPanel();

	Render::JViewport viewport = { 0, 0, static_cast<float>(clientWidth), static_cast<float>(clientHeight), 0, 1 };
	D3D12_RECT rect = CD3DX12_RECT(0, 0, static_cast<LONG>(clientWidth), static_cast<LONG>(clientHeight));

	Engine::JRenderer::FrameDesc frameDesc;
	if (!renderServer->BuildFrameDesc(*scene, swapChain->GetRenderTarget(), JColors::DarkGray, viewport, rect, frameDesc))
	{
		std::cerr << "JScenePanel::Update skipped: failed to build frame description." << std::endl;
		return;
	}

	renderer->Render(frameDesc);
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

	Engine::JScene::TransformData* transformData = scene->GetTransform(cameraData->entity);
	if (transformData == nullptr)
	{
		return;
	}

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
		transformData->rotation.x,
		transformData->rotation.y,
		transformData->rotation.z
	};
	newRotation.y += yawInput;
	newRotation.x += pitchInput;

	const XMVECTOR forwardVector = getForward(newRotation.y, newRotation.x);
	const XMVECTOR rightVector = getRight(newRotation.y);
	const XMVECTOR upVector = getUp(newRotation.y, newRotation.x);
	XMVECTOR position = XMVectorSet(transformData->translation.x, transformData->translation.y, transformData->translation.z, 1.0f);
	position += forwardVector * (forwardInput * moveSpeed * deltaTime);
	position += rightVector * (rightInput * moveSpeed * deltaTime);
	position += upVector * (upInput * moveSpeed * deltaTime);

	XMFLOAT3 newPosition;
	XMStoreFloat3(&newPosition, position);

	Engine::JCameraSystem* cameraSystem = GetEngine()->GetCameraSystem();
	if (cameraSystem != nullptr)
	{
		cameraSystem->SetRotate(*scene, _sceneCamera, newRotation.x, newRotation.y, newRotation.z);
		cameraSystem->SetPosition(*scene, _sceneCamera, newPosition.x, newPosition.y, newPosition.z);
	}
}

#pragma region Debug Panel

void JScenePanel::createCameraInfoPanel()
{
	if (_mainWindow == nullptr || _cameraInfoWindow != nullptr)
	{
		return;
	}

	RECT mainRect{};
	GetWindowRect(_mainWindow, &mainRect);

	_cameraInfoWindow = CreateWindowExW(
		WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
		L"STATIC",
		L"Camera Info",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
		mainRect.right + 16,
		mainRect.top,
		360,
		460,
		_mainWindow,
		nullptr,
		GetModuleHandleW(nullptr),
		nullptr);

	if (_cameraInfoWindow == nullptr)
	{
		return;
	}

	_cameraInfoText = CreateWindowExW(
		0,
		L"STATIC",
		L"",
		WS_CHILD | WS_VISIBLE | SS_LEFT,
		12,
		12,
		320,
		400,
		_cameraInfoWindow,
		nullptr,
		GetModuleHandleW(nullptr),
		nullptr);

	if (_cameraInfoText != nullptr)
	{
		HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
		SendMessageW(_cameraInfoText, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
	}
}

void JScenePanel::updateCameraInfoPanel()
{
	Engine::JScene* scene = getScene();
	if (_cameraInfoWindow == nullptr || _cameraInfoText == nullptr || scene == nullptr || !_sceneCamera.IsValid())
	{
		return;
	}

	if (_mainWindow != nullptr)
	{
		RECT mainRect{};
		GetWindowRect(_mainWindow, &mainRect);
		SetWindowPos(_cameraInfoWindow, HWND_TOPMOST, mainRect.right + 16, mainRect.top, 360, 460, SWP_NOACTIVATE);
	}

	const Engine::JScene::CameraData* cameraData = scene->GetCamera(_sceneCamera);
	const Engine::JScene::TransformData* transformData = cameraData != nullptr ? scene->GetTransform(cameraData->entity) : nullptr;
	if (transformData == nullptr)
	{
		return;
	}

	std::wostringstream stream;
	stream << std::fixed << std::setprecision(2);
	stream << L"World Translate\n";
	stream << L"X: " << transformData->translation.x << L"\n";
	stream << L"Y: " << transformData->translation.y << L"\n";
	stream << L"Z: " << transformData->translation.z << L"\n\n";
	stream << L"World Rotation\n";
	stream << L"X: " << wrapDegrees(toDegrees(transformData->rotation.x)) << L"\n";
	stream << L"Y: " << wrapDegrees(toDegrees(transformData->rotation.y)) << L"\n";
	stream << L"Z: " << wrapDegrees(toDegrees(transformData->rotation.z)) << L"\n\n";
	stream << L"World Scale\n";
	stream << L"X: " << transformData->scale.x << L"\n";
	stream << L"Y: " << transformData->scale.y << L"\n";
	stream << L"Z: " << transformData->scale.z << L"\n\n";
	stream << L"Editor Camera\n";
	stream << L"Base Speed: " << _editorCameraMoveSpeed << L"\n\n";
	stream << L"Projection\n";
	stream << L"Aspect: " << cameraData->aspectRatio << L"\n";
	stream << L"Near: " << cameraData->nearP << L"\n";
	stream << L"Far: " << cameraData->farP << L"\n\n";

	const Engine::JScene::LightData* lightData = scene->GetLight(_light);
	const Engine::JScene::TransformData* lightTransformData = lightData != nullptr ? scene->GetTransform(lightData->entity) : nullptr;
	stream << L"Scene Light\n";
	if (lightData != nullptr && lightTransformData != nullptr)
	{
		stream << L"Count: 1\n";
		stream << L"Pos: "
			<< lightTransformData->translation.x << L", "
			<< lightTransformData->translation.y << L", "
			<< lightTransformData->translation.z << L"\n";
		stream << L"Color: "
			<< lightData->color.x << L", "
			<< lightData->color.y << L", "
			<< lightData->color.z << L"\n";
		stream << L"Intensity: " << lightData->intensity << L"\n\n";
	}
	else
	{
		stream << L"Count: 0\n\n";
	}

	const Engine::JRenderServer* renderServer = GetEngine() != nullptr ? GetEngine()->GetRenderServer() : nullptr;
	const Engine::JRenderDB::LightResource* lightResource = renderServer != nullptr ? renderServer->GetRenderDB().GetLightResource() : nullptr;
	stream << L"RenderDB Light\n";
	if (lightResource != nullptr)
	{
		stream << L"GPU Buffer: " << (lightResource->lightBuffer != nullptr ? L"Ready" : L"Null") << L"\n";
		stream << L"Count: " << lightResource->lightCount << L"\n";
		stream << L"Pos: "
			<< lightResource->positionCount.x << L", "
			<< lightResource->positionCount.y << L", "
			<< lightResource->positionCount.z << L"\n";
		stream << L"Intensity: " << lightResource->colorIntensity.w;
	}
	else
	{
		stream << L"Unavailable";
	}

	SetWindowTextW(_cameraInfoText, stream.str().c_str());
}

#pragma endregion

J_EDITOR_END
