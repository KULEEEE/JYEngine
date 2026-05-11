#include "client/editor/JScenePanel.h"

#include "engine/JSwapChain.h"
#include "engine/JRenderDefinition.h"
#include "engine/JRenderResource.h"
#include "engine/JRenderServer.h"
#include "engine/JRenderer.h"
#include "engine/JMaterialFactory.h"
#include "engine/JCameraComponent.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"

#include "client/editor/JFBXLoader.h"

#include <iostream>
#include <iomanip>
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
	};

	constexpr float FIXED_DELTA_TIME = 1.0f / 60.0f;
	constexpr float MOUSE_LOOK_SENSITIVITY = 0.0025f;
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

	if (renderServer != nullptr && camera != nullptr)
	{
		renderServer->UnregisterCamera(camera->instanceID);
	}

	if (_cameraInfoWindow != nullptr)
	{
		DestroyWindow(_cameraInfoWindow);
		_cameraInfoWindow = nullptr;
		_cameraInfoText = nullptr;
	}

	delete material;
	delete planeMaterial;
	delete perFrameBuffer;
	delete materialBuffer;
	delete materialTexture;
	delete camera;
	delete mesh;
	delete planeMesh;

	material = nullptr;
	planeMaterial = nullptr;
	perFrameBuffer = nullptr;
	materialBuffer = nullptr;
	materialTexture = nullptr;
	camera = nullptr;
	meshResource = nullptr;
	planeMeshResource = nullptr;
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

	camera = new Engine::JCameraComponent();
	if (camera == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: camera creation failed." << std::endl;
		return;
	}

	camera->SetMoveSpeed(0.1f);
	camera->SetRotateSpeed(1.0f);

	PerFrameConstants perFrameConstants{};
	XMStoreFloat4x4(&perFrameConstants.viewProjection, XMMatrixIdentity());
	perFrameBuffer = materialFactory->CreateConstantBuffer(&perFrameConstants, sizeof(perFrameConstants));
	if (perFrameBuffer == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: per-frame buffer creation failed." << std::endl;
		return;
	}
	material->SetConstantBuffer("PerFrame", perFrameBuffer);
	renderServer->RegisterCamera(camera, perFrameBuffer, 800.0f / 600.0f);

	MaterialConstants materialConstants{};
	materialConstants.baseColor = JVec4(1.0f, 1.0f, 1.0f, 1.0f);
	materialBuffer = materialFactory->CreateAndSetConstantBuffer(material, "PerMaterial", &materialConstants, sizeof(materialConstants));
	if (materialBuffer == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: material buffer creation failed." << std::endl;
		return;
	}

	materialTexture = materialFactory->CreateAndSetSolidColorTexture(material, "BaseTexture", JColor(0.0f, 0.0f, 1.0f, 1.0f));
	if (materialTexture == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: material texture creation failed." << std::endl;
		return;
	}
	planeMaterial->SetConstantBuffer("PerFrame", perFrameBuffer);

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

	planeMeshResource = renderServer->GetRenderDB().CreateOrUpdateMeshResource(planeMesh);
	if (planeMeshResource == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: plane buffer creation failed." << std::endl;
		return;
	}

	renderServer->RegisterMaterial(material);
	renderServer->RegisterMaterial(planeMaterial);
	renderServer->Sync();

	std::string meshPath = get_Engine_Mesh_Path() + "\\Car.fbx";
	std::cout << "Loading mesh: " << meshPath << std::endl;

	JFBXLoader fbxLoader;
	mesh = fbxLoader.LoadFBX(meshPath.c_str());
	if (mesh == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: mesh load failed." << std::endl;
		return;
	}

	meshResource = renderServer->GetRenderDB().CreateOrUpdateMeshResource(mesh);
	if (meshResource == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: buffer creation failed." << std::endl;
		return;
	}

	createCameraInfoPanel();
	updateCameraInfoPanel();
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
	if (swapChain == nullptr || renderServer == nullptr || renderer == nullptr || material == nullptr || planeMaterial == nullptr || camera == nullptr || mesh == nullptr || planeMesh == nullptr || meshResource == nullptr || planeMeshResource == nullptr)
	{
		std::cerr << "JScenePanel::Update skipped: render resources are not ready." << std::endl;
		_isReady = false;
		return;
	}

	updateCamera(FIXED_DELTA_TIME);
	renderServer->MarkCameraDirty(camera);
	renderServer->MarkMaterialDirty(material);
	renderServer->MarkMaterialDirty(planeMaterial);
	renderServer->Sync();
	updateCameraInfoPanel();

	Render::JViewport viewport = { 0, 0, 800, 600, 0, 1 };
	D3D12_RECT rect = CD3DX12_RECT(0, 0, 800, 600);

	Engine::JRenderer::FrameDesc frameDesc;
	frameDesc.cameraID = camera->instanceID;
	frameDesc.renderTarget = swapChain->GetRenderTarget();
	frameDesc.clearColor = JColors::DarkGray;
	frameDesc.viewport = viewport;
	frameDesc.scissorRect = rect;
	frameDesc.drawItems.push_back({ planeMaterial->instanceID, planeMeshResource });
	frameDesc.drawItems.push_back({ material->instanceID, meshResource });

	renderer->Render(frameDesc);
}

void JScenePanel::updateCamera(float deltaTime)
{
	if (camera == nullptr)
	{
		return;
	}

	float yawInput = 0.0f;
	float pitchInput = 0.0f;
	float forwardInput = 0.0f;
	float rightInput = 0.0f;
	float upInput = 0.0f;

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

	camera->Rotate(yawInput, pitchInput);
	camera->MoveLocal(forwardInput, rightInput, upInput, deltaTime);
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
		320,
		260,
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
		280,
		210,
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
	if (_cameraInfoWindow == nullptr || _cameraInfoText == nullptr || camera == nullptr)
	{
		return;
	}

	if (_mainWindow != nullptr)
	{
		RECT mainRect{};
		GetWindowRect(_mainWindow, &mainRect);
		SetWindowPos(_cameraInfoWindow, HWND_TOPMOST, mainRect.right + 16, mainRect.top, 320, 260, SWP_NOACTIVATE);
	}

	const JVec3& position = camera->GetPosition();
	const float yawDegrees = wrapDegrees(toDegrees(camera->GetYaw()));
	const float pitchDegrees = wrapDegrees(toDegrees(camera->GetPitch()));

	std::wostringstream stream;
	stream << std::fixed << std::setprecision(2);
	stream << L"World Position\n";
	stream << L"X: " << position.x << L"\n";
	stream << L"Y: " << position.y << L"\n";
	stream << L"Z: " << position.z << L"\n\n";
	stream << L"World Rotation\n";
	stream << L"Pitch(X): " << pitchDegrees << L"\n";
	stream << L"Yaw(Y): " << yawDegrees << L"\n";
	stream << L"Roll(Z): 0.00";

	SetWindowTextW(_cameraInfoText, stream.str().c_str());
}

J_EDITOR_END
