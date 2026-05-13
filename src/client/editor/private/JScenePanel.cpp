#include "client/editor/JScenePanel.h"

#include "engine/render/JSwapChain.h"
#include "engine/render/JRenderDefinition.h"
#include "engine/render/JRenderDB.h"
#include "engine/render/JRenderServer.h"
#include "engine/render/JRenderer.h"
#include "engine/render/JMaterialFactory.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"

#include "client/editor/JFBXLoader.h"

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <sstream>

J_EDITOR_BEGIN

namespace
{
	struct PerFrameConstants
	{
		XMFLOAT4X4 viewProjection;
	};

	struct MaterialConstants
	{
		JVec4 baseColor;
		float roughness = 0.5f;
		float metallic = 1.0f;
		JVec2 padding = {};
	};

	constexpr float MOUSE_LOOK_SENSITIVITY = 0.0025f;
	constexpr float MAX_FRAME_DELTA_TIME = 0.1f;
	constexpr float CAMERA_SPEED_MIN = 0.5f;
	constexpr float CAMERA_SPEED_MAX = 100.0f;
	constexpr float CAMERA_SPEED_STEP_MULTIPLIER = 1.2f;
	constexpr float CAMERA_SPEED_SHIFT_MULTIPLIER = 4.0f;
	constexpr float PLANE_SIZE = 5000.0f;
	constexpr float PLANE_Y = -0.1f;

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

JScenePanel::~JScenePanel()
{
	J::Engine::JRenderServer* renderServer = GetEngine() != nullptr ? GetEngine()->GetRenderServer() : nullptr;
	if (renderServer != nullptr && material != nullptr)
	{
		renderServer->UnregisterMaterial(material->instanceID);
	}

	if (renderServer != nullptr && planeMaterial != nullptr)
	{
		renderServer->UnregisterMaterial(planeMaterial->instanceID);
	}

	if (renderServer != nullptr && _camera.IsValid())
	{
		renderServer->UnregisterCamera(_camera);
	}

	if (renderServer != nullptr && mesh != nullptr)
	{
		renderServer->GetRenderDB().RemoveMeshResource(mesh);
	}

	if (renderServer != nullptr && planeMesh != nullptr)
	{
		renderServer->GetRenderDB().RemoveMeshResource(planeMesh);
	}

	if (_cameraInfoWindow != nullptr)
	{
		DestroyWindow(_cameraInfoWindow);
		_cameraInfoWindow = nullptr;
		_cameraInfoText = nullptr;
	}

	delete scene;
	delete material;
	delete planeMaterial;
	delete perFrameBuffer;
	delete materialBuffer;
	delete baseColorTexture;
	delete normalTexture;
	delete roughnessTexture;
	delete metallicTexture;
	delete mesh;
	delete planeMesh;

	scene = nullptr;
	material = nullptr;
	planeMaterial = nullptr;
	perFrameBuffer = nullptr;
	materialBuffer = nullptr;
	baseColorTexture = nullptr;
	normalTexture = nullptr;
	roughnessTexture = nullptr;
	metallicTexture = nullptr;
	mesh = nullptr;
	planeMesh = nullptr;
}

void JScenePanel::Init()
{
	_commandQueue = GetEngine()->GetCmdQueue();
	_mainWindow = GetActiveWindow();
	if (_mainWindow == nullptr)
	{
		_mainWindow = GetForegroundWindow();
	}

	Engine::JRenderServer* renderServer = GetEngine()->GetRenderServer();
	Engine::JMaterialFactory* materialFactory = GetEngine()->GetMaterialFactory();
	if (renderServer == nullptr || materialFactory == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: render server or material factory is null." << std::endl;
		return;
	}

	scene = new Engine::JScene();
	if (scene == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: scene creation failed." << std::endl;
		return;
	}

	std::string baseShaderPath = get_Engine_Shader_Path() + "\\base.hlsl";
	material = materialFactory->CreateMaterial(baseShaderPath);
	if (material == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: material creation failed." << std::endl;
		return;
	}

	std::string gridShaderPath = get_Engine_Shader_Path() + "\\grid.hlsl";
	planeMaterial = materialFactory->CreateMaterial(gridShaderPath, true);
	if (planeMaterial == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: plane material creation failed." << std::endl;
		return;
	}

	PerFrameConstants perFrameConstants{};
	XMStoreFloat4x4(&perFrameConstants.viewProjection, XMMatrixIdentity());
	perFrameBuffer = materialFactory->CreateConstantBuffer(&perFrameConstants, sizeof(perFrameConstants));
	if (perFrameBuffer == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: per-frame buffer creation failed." << std::endl;
		return;
	}

	material->SetConstantBuffer("PerFrame", perFrameBuffer);
	planeMaterial->SetConstantBuffer("PerFrame", perFrameBuffer);

	MaterialConstants materialConstants{};
	materialConstants.baseColor = JVec4(1.0f, 1.0f, 1.0f, 1.0f);
	materialBuffer = materialFactory->CreateAndSetConstantBuffer(material, "PerMaterial", &materialConstants, sizeof(materialConstants));
	if (materialBuffer == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: material buffer creation failed." << std::endl;
		return;
	}

	const std::string texturePath = get_Engine_Res_Path() + "\\texture";
	baseColorTexture = materialFactory->CreateAndSetTextureFromFile(material, "BaseTexture", texturePath + "\\Base_color.png");
	normalTexture = materialFactory->CreateAndSetTextureFromFile(material, "NormalTexture", texturePath + "\\normal.png");
	roughnessTexture = materialFactory->CreateAndSetTextureFromFile(material, "RoughnessTexture", texturePath + "\\roughness.png");
	metallicTexture = materialFactory->CreateAndSetTextureFromFile(material, "MetallicTexture", texturePath + "\\metallic.png");
	if (baseColorTexture == nullptr || normalTexture == nullptr || roughnessTexture == nullptr || metallicTexture == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: PBR texture creation failed." << std::endl;
		return;
	}

	renderServer->RegisterMaterial(material);
	renderServer->RegisterMaterial(planeMaterial);
	renderServer->Sync();

	_cameraEntity = scene->CreateEntity();
	Engine::JScene::TransformData cameraTransformData{};
	cameraTransformData.position = { 0.0f, 0.0f, -8.0f };
	_cameraTransform = scene->AddTransform(_cameraEntity, cameraTransformData);
	const uint32 initialWidth = getClientWidth(_mainWindow);
	const uint32 initialHeight = getClientHeight(_mainWindow);
	_viewportWidth = initialWidth;
	_viewportHeight = initialHeight;
	_camera = scene->AddCamera(_cameraEntity, _cameraTransform, static_cast<float>(initialWidth) / static_cast<float>(initialHeight));
	if (!_camera.IsValid())
	{
		std::cerr << "JScenePanel::Init failed: camera creation failed." << std::endl;
		return;
	}

	Engine::JScene::CameraData* cameraData = scene->GetCamera(_camera);
	if (cameraData == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: camera data access failed." << std::endl;
		return;
	}
	cameraData->moveSpeed = _editorCameraMoveSpeed;
	cameraData->rotateSpeed = 1.0f;
	scene->SetPrimaryCamera(_camera);
	renderServer->RegisterCamera(_camera, perFrameBuffer);

	_lightEntity = scene->CreateEntity();
	Engine::JScene::TransformData lightTransformData{};
	lightTransformData.position = { 0.0f, 4.0f, -4.0f };
	_lightTransform = scene->AddTransform(_lightEntity, lightTransformData);
	Engine::JScene::LightData lightData{};
	lightData.color = { 1.0f, 0.9f, 0.65f };
	lightData.intensity = 3.0f;
	_light = scene->AddLight(_lightEntity, _lightTransform, lightData);

	std::vector<float> planePositions =
	{
		-PLANE_SIZE, PLANE_Y, -PLANE_SIZE, 1.0f,
		 PLANE_SIZE, PLANE_Y, -PLANE_SIZE, 1.0f,
		 PLANE_SIZE, PLANE_Y,  PLANE_SIZE, 1.0f,
		-PLANE_SIZE, PLANE_Y,  PLANE_SIZE, 1.0f,
	};

	std::vector<uint32> planeIndices = { 0, 1, 2, 0, 2, 3 };
	planeMesh = new Engine::JMesh();
	if (planeMesh == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: plane mesh creation failed." << std::endl;
		return;
	}
	planeMesh->SetPositions(std::move(planePositions));
	planeMesh->SetIndices(std::move(planeIndices));

	_planeEntity = scene->CreateEntity();
	_planeTransform = scene->AddTransform(_planeEntity);
	_planeRenderObject = scene->AddRenderObject(_planeEntity, _planeTransform, planeMaterial->instanceID, planeMesh, true);

	std::string meshPath = get_Engine_Mesh_Path() + "\\Cup.fbx";
	std::cout << "Loading mesh: " << meshPath << std::endl;

	JFBXLoader fbxLoader;
	mesh = fbxLoader.LoadFBX(meshPath.c_str());
	if (mesh == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: mesh load failed." << std::endl;
		return;
	}

	_carEntity = scene->CreateEntity();
	Engine::JScene::TransformData cupTransformData{};
	cupTransformData.pitch = 1.507f;
	_carTransform = scene->AddTransform(_carEntity, cupTransformData);
	_carRenderObject = scene->AddRenderObject(_carEntity, _carTransform, material->instanceID, mesh, false);

	renderServer->SyncScene(*scene);

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
	if (swapChain == nullptr || renderServer == nullptr || renderer == nullptr || scene == nullptr || material == nullptr || planeMaterial == nullptr || mesh == nullptr || planeMesh == nullptr || !_camera.IsValid())
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

		Engine::JScene::CameraData* cameraData = scene->GetCamera(_camera);
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

	updateCamera(deltaTime);
	renderServer->MarkCameraDirty(_camera);
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

	if (scene == nullptr || !_camera.IsValid())
	{
		return;
	}

	Engine::JScene::CameraData* cameraData = scene->GetCamera(_camera);
	if (cameraData != nullptr)
	{
		cameraData->moveSpeed = _editorCameraMoveSpeed;
	}
}

void JScenePanel::updateCamera(float deltaTime)
{
	if (scene == nullptr || !_camera.IsValid())
	{
		return;
	}

	if (_mainWindow == nullptr || GetForegroundWindow() != _mainWindow)
	{
		_isMouseLookActive = false;
		return;
	}

	Engine::JScene::CameraData* cameraData = scene->GetCamera(_camera);
	if (cameraData == nullptr)
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

	cameraData->moveSpeed = _editorCameraMoveSpeed * moveSpeedMultiplier;
	scene->RotateCamera(_camera, yawInput, pitchInput);
	scene->MoveCameraLocal(_camera, forwardInput, rightInput, upInput, deltaTime);
	cameraData->moveSpeed = _editorCameraMoveSpeed;
}

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
	if (_cameraInfoWindow == nullptr || _cameraInfoText == nullptr || scene == nullptr || !_camera.IsValid())
	{
		return;
	}

	if (_mainWindow != nullptr)
	{
		RECT mainRect{};
		GetWindowRect(_mainWindow, &mainRect);
		SetWindowPos(_cameraInfoWindow, HWND_TOPMOST, mainRect.right + 16, mainRect.top, 360, 460, SWP_NOACTIVATE);
	}

	const Engine::JScene::CameraData* cameraData = scene->GetCamera(_camera);
	const Engine::JScene::TransformData* transformData = cameraData != nullptr ? scene->GetTransform(cameraData->transform) : nullptr;
	if (transformData == nullptr)
	{
		return;
	}

	std::wostringstream stream;
	stream << std::fixed << std::setprecision(2);
	stream << L"World Position\n";
	stream << L"X: " << transformData->position.x << L"\n";
	stream << L"Y: " << transformData->position.y << L"\n";
	stream << L"Z: " << transformData->position.z << L"\n\n";
	stream << L"World Rotation\n";
	stream << L"Pitch(X): " << wrapDegrees(toDegrees(transformData->pitch)) << L"\n";
	stream << L"Yaw(Y): " << wrapDegrees(toDegrees(transformData->yaw)) << L"\n";
	stream << L"Roll(Z): 0.00\n\n";
	stream << L"Editor Camera\n";
	stream << L"Move Speed: " << cameraData->moveSpeed << L"\n";
	stream << L"Base Speed: " << _editorCameraMoveSpeed << L"\n\n";

	const Engine::JScene::LightData* lightData = scene->GetLight(_light);
	const Engine::JScene::TransformData* lightTransformData = lightData != nullptr ? scene->GetTransform(lightData->transform) : nullptr;
	stream << L"Scene Light\n";
	if (lightData != nullptr && lightTransformData != nullptr)
	{
		stream << L"Count: 1\n";
		stream << L"Pos: "
			<< lightTransformData->position.x << L", "
			<< lightTransformData->position.y << L", "
			<< lightTransformData->position.z << L"\n";
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

J_EDITOR_END




