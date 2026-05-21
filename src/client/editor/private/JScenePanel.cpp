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
	constexpr float EDITOR_GRID_SIZE = 5000.0f;
	constexpr float EDITOR_GRID_Y = -0.1f;

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

	createEditorGrid();
	selectDefaultRenderObject();

	Engine::JRenderer* renderer = GetEngine()->GetRenderer();
	if (renderer != nullptr)
	{
		renderer->SetRenderPath(Engine::JRenderer::RenderPath::Deferred);
	}

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
	updateSelectedObject(deltaTime);
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

	populateDebugOverlay(frameDesc, deltaTime);
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
			renderObject->visible = !renderObject->visible;
			scene->MarkRenderObjectComponentModified(_selectedEntity);
		}
	}
}

void JScenePanel::populateDebugOverlay(Engine::JFrameDesc& frameDesc, float deltaTime)
{
	Engine::JScene* scene = getScene();
	if (scene == nullptr || !_sceneCamera.IsValid())
	{
		return;
	}

	const Engine::JScene::CameraData* cameraData = scene->GetCamera(_sceneCamera);
	const Engine::JTransformHandle transformHandle = cameraData != nullptr ? scene->GetTransformHandle(cameraData->entity) : Engine::JTransformHandle{};
	if (!transformHandle.IsValid())
	{
		return;
	}

	const Engine::JScene::TransformData transformData = scene->GetTransform(transformHandle);

	uint32 lightCount = 0;
	for (const Engine::JScene::LightSlot& slot : scene->GetLightSlots())
	{
		if (slot.active && slot.data.active)
		{
			++lightCount;
		}
	}

	uint32 objectCount = 0;
	for (const Engine::JScene::RenderObjectComponentSlot& slot : scene->GetRenderObjectComponentSlots())
	{
		if (slot.active && slot.data.active && slot.data.visible)
		{
			++objectCount;
		}
	}

	const float fps = deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f;
	const Engine::JScene::LightData* lightData = scene->GetLight(_light);
	const Engine::JTransformHandle lightTransformHandle = lightData != nullptr ? scene->GetTransformHandle(lightData->entity) : Engine::JTransformHandle{};

	std::ostringstream stream;
	stream << std::fixed << std::setprecision(2);
	stream << "FPS " << fps << "  DT " << (deltaTime * 1000.0f) << "MS";
	frameDesc.debugOverlayLines.push_back(stream.str());

	stream.str("");
	stream.clear();
	stream << "DRAW O " << frameDesc.opaqueDrawItems.size()
		<< "  T " << frameDesc.transparentDrawItems.size()
		<< "  OBJ " << objectCount;
	frameDesc.debugOverlayLines.push_back(stream.str());

	stream.str("");
	stream.clear();
	stream << "CULL TEST " << frameDesc.cullingTestedDrawItemCount
		<< "  CULLED " << frameDesc.culledDrawItemCount
		<< "  VISIBLE " << (frameDesc.opaqueDrawItems.size() + frameDesc.transparentDrawItems.size());
	frameDesc.debugOverlayLines.push_back(stream.str());

	stream.str("");
	stream.clear();
	stream << "CAM POS "
		<< transformData.translation.x << ", "
		<< transformData.translation.y << ", "
		<< transformData.translation.z;
	frameDesc.debugOverlayLines.push_back(stream.str());

	stream.str("");
	stream.clear();
	stream << "CAM ROT "
		<< wrapDegrees(toDegrees(transformData.rotation.x)) << ", "
		<< wrapDegrees(toDegrees(transformData.rotation.y)) << ", "
		<< wrapDegrees(toDegrees(transformData.rotation.z));
	frameDesc.debugOverlayLines.push_back(stream.str());

	stream.str("");
	stream.clear();
	stream << "CAM SPEED " << _editorCameraMoveSpeed
		<< "  NEAR " << (cameraData != nullptr ? cameraData->nearP : 0.0f)
		<< "  FAR " << (cameraData != nullptr ? cameraData->farP : 0.0f);
	frameDesc.debugOverlayLines.push_back(stream.str());

	if (_selectedEntity.IsValid())
	{
		const Engine::JTransformHandle selectedTransformHandle = scene->GetTransformHandle(_selectedEntity);
		if (selectedTransformHandle.IsValid())
		{
			const Engine::JScene::TransformData selectedTransform = scene->GetTransform(selectedTransformHandle);
			stream.str("");
			stream.clear();
			stream << "SEL POS "
				<< selectedTransform.translation.x << ", "
				<< selectedTransform.translation.y << ", "
				<< selectedTransform.translation.z
				<< "  V TOGGLE";
			frameDesc.debugOverlayLines.push_back(stream.str());
		}
	}

	stream.str("");
	stream.clear();
	stream << "LIGHTS " << lightCount;
	if (lightData != nullptr && lightTransformHandle.IsValid())
	{
		const Engine::JScene::TransformData lightTransformData = scene->GetTransform(lightTransformHandle);
		stream << "  FIRST "
			<< lightTransformData.translation.x << ", "
			<< lightTransformData.translation.y << ", "
			<< lightTransformData.translation.z;
	}
	frameDesc.debugOverlayLines.push_back(stream.str());
}

J_EDITOR_END
