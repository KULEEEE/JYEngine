#include "client/editor/JScenePanel.h"

#include "engine/JSwapChain.h"
#include "engine/JGraphicResource.h"
#include "engine/JRenderDefinition.h"
#include "engine/JRenderResource.h"
#include "engine/JRenderContext.h"
#include "engine/JRenderServer.h"
#include "engine/JCameraComponent.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JShader.h"
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

	if (_cameraInfoWindow != nullptr)
	{
		DestroyWindow(_cameraInfoWindow);
		_cameraInfoWindow = nullptr;
		_cameraInfoText = nullptr;
	}

	delete shader;
	delete pipeline;
	delete planeShader;
	delete planePipeline;
	delete graphicResource;
	delete planeGraphicResource;
	delete material;
	delete planeMaterial;
	delete perFrameBuffer;
	delete materialBuffer;
	delete materialTexture;
	delete camera;
	delete vertexBuffer;
	delete indexBuffer;
	delete planeVertexBuffer;
	delete planeIndexBuffer;
	delete mesh;
	delete planeMesh;

	shader = nullptr;
	pipeline = nullptr;
	planeShader = nullptr;
	planePipeline = nullptr;
	graphicResource = nullptr;
	planeGraphicResource = nullptr;
	material = nullptr;
	planeMaterial = nullptr;
	perFrameBuffer = nullptr;
	materialBuffer = nullptr;
	materialTexture = nullptr;
	camera = nullptr;
	vertexBuffer = nullptr;
	indexBuffer = nullptr;
	planeVertexBuffer = nullptr;
	planeIndexBuffer = nullptr;
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

	Render::JRenderContext* renderContext = GetEngine()->GetRenderContext();
	Engine::JRenderServer* renderServer = GetEngine()->GetRenderServer();
	if (renderContext == nullptr || renderServer == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: render context or render server is null." << std::endl;
		return;
	}

	std::string baseShaderPath = get_Engine_Shader_Path() + "\\base.hlsl";
	shader = renderContext->CreateShader(baseShaderPath.c_str());
	if (shader == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: shader load failed." << std::endl;
		return;
	}

	std::string gridShaderPath = get_Engine_Shader_Path() + "\\grid.hlsl";
	planeShader = renderContext->CreateShader(gridShaderPath.c_str());
	if (planeShader == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: grid shader load failed." << std::endl;
		return;
	}

	pipeline = renderContext->CreatePipeline(shader);
	if (pipeline == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: pipeline creation failed." << std::endl;
		return;
	}

	planePipeline = renderContext->CreatePipeline(planeShader, true);
	if (planePipeline == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: grid pipeline creation failed." << std::endl;
		return;
	}

	material = new Engine::JMaterial();
	planeMaterial = new Engine::JMaterial();
	camera = new Engine::JCameraComponent();
	graphicResource = new Render::JGraphicResource(shader);
	planeGraphicResource = new Render::JGraphicResource(planeShader);
	if (material == nullptr || planeMaterial == nullptr || camera == nullptr || graphicResource == nullptr || planeGraphicResource == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: material, camera, or graphic resource creation failed." << std::endl;
		return;
	}

	camera->SetMoveSpeed(0.1f);
	camera->SetRotateSpeed(1.0f);

	PerFrameConstants perFrameConstants{};
	XMStoreFloat4x4(&perFrameConstants.viewProjection, XMMatrixIdentity());
	perFrameBuffer = renderContext->CreateConstantBuffer(&perFrameConstants, sizeof(perFrameConstants));
	if (perFrameBuffer == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: per-frame buffer creation failed." << std::endl;
		return;
	}
	material->SetConstantBuffer("PerFrame", perFrameBuffer);

	MaterialConstants materialConstants{};
	materialConstants.baseColor = JVec4(1.0f, 1.0f, 1.0f, 1.0f);
	materialBuffer = renderContext->CreateConstantBuffer(&materialConstants, sizeof(materialConstants));
	if (materialBuffer == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: material buffer creation failed." << std::endl;
		return;
	}
	material->SetConstantBuffer("PerMaterial", materialBuffer);

	materialTexture = renderContext->CreateSolidColorTexture(JColor(0.0f, 0.0f, 1.0f, 1.0f));
	if (materialTexture == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: material texture creation failed." << std::endl;
		return;
	}
	material->SetTexture("BaseTexture", materialTexture);

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

	planeVertexBuffer = renderContext->CreateVertexBuffer(planeMesh->GetPositions(), planeMesh->GetVertexCount());
	planeIndexBuffer = renderContext->CreateIndexBuffer(planeMesh->GetIndices());
	if (planeVertexBuffer == nullptr || planeIndexBuffer == nullptr)
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

	vertexBuffer = renderContext->CreateVertexBuffer(mesh->GetPositions(), mesh->GetVertexCount());
	indexBuffer = renderContext->CreateIndexBuffer(mesh->GetIndices());
	if (vertexBuffer == nullptr || indexBuffer == nullptr)
	{
		std::cerr << "JScenePanel::Init failed: buffer creation failed." << std::endl;
		return;
	}

	updatePerFrameBuffer();
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
	Render::JRenderContext* renderContext = GetEngine()->GetRenderContext();
	if (swapChain == nullptr || renderServer == nullptr || renderContext == nullptr || pipeline == nullptr || planePipeline == nullptr || shader == nullptr || planeShader == nullptr || material == nullptr || planeMaterial == nullptr || camera == nullptr || graphicResource == nullptr || planeGraphicResource == nullptr || mesh == nullptr || planeMesh == nullptr || vertexBuffer == nullptr || indexBuffer == nullptr || planeVertexBuffer == nullptr || planeIndexBuffer == nullptr)
	{
		std::cerr << "JScenePanel::Update skipped: render resources are not ready." << std::endl;
		_isReady = false;
		return;
	}

	updateCamera(FIXED_DELTA_TIME);
	updatePerFrameBuffer();
	renderServer->MarkMaterialDirty(material);
	renderServer->MarkMaterialDirty(planeMaterial);
	renderServer->Sync();
	updateCameraInfoPanel();

	_commandQueue->BeginRenderPass(swapChain->GetRenderTarget(), JColors::DarkGray, 0);

	Render::JViewport viewport = { 0, 0, 800, 600, 0, 1 };
	D3D12_RECT rect = CD3DX12_RECT(0, 0, 800, 600);

	Engine::JMeshResource meshResource;
	meshResource.soaBuffers.push_back(vertexBuffer->view);
	meshResource.indexBuffer = indexBuffer->view;
	meshResource.indexSize = mesh->GetIndices().size();

	Engine::JMeshResource planeMeshResource;
	planeMeshResource.soaBuffers.push_back(planeVertexBuffer->view);
	planeMeshResource.indexBuffer = planeIndexBuffer->view;
	planeMeshResource.indexSize = planeMesh->GetIndices().size();

	if (!renderServer->BuildGraphicResource(material->instanceID, shader, *graphicResource))
	{
		std::cerr << "JScenePanel::Update skipped: failed to build graphic resource." << std::endl;
		return;
	}

	if (!renderServer->BuildGraphicResource(planeMaterial->instanceID, planeShader, *planeGraphicResource))
	{
		std::cerr << "JScenePanel::Update skipped: failed to build plane graphic resource." << std::endl;
		return;
	}

	_commandQueue->SetViewports(1, &viewport);
	_commandQueue->SetScissorRects(1, &rect);
	_commandQueue->SetPipeline(planePipeline);
	_commandQueue->SetGraphicResources(planeGraphicResource);
	_commandQueue->BindVertexBuffer(&planeMeshResource);
	_commandQueue->DrawIndexed(static_cast<uint32>(planeMeshResource.indexSize), 1, 0, 0, 0);
	_commandQueue->SetPipeline(pipeline);
	_commandQueue->SetGraphicResources(graphicResource);
	_commandQueue->BindVertexBuffer(&meshResource);
	_commandQueue->DrawIndexed(static_cast<uint32>(meshResource.indexSize), 1, 0, 0, 0);
	_commandQueue->EndRenderPass();
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

void JScenePanel::updatePerFrameBuffer()
{
	Render::JRenderContext* renderContext = GetEngine()->GetRenderContext();
	if (renderContext == nullptr || perFrameBuffer == nullptr || camera == nullptr)
	{
		return;
	}

	const XMMATRIX view = camera->GetViewMatrix();
	const XMMATRIX proj = camera->GetProjectionMatrix(800.0f / 600.0f);
	const XMMATRIX viewProj = XMMatrixTranspose(view * proj);

	PerFrameConstants perFrameConstants{};
	XMStoreFloat4x4(&perFrameConstants.viewProjection, viewProj);
	renderContext->UpdateConstantBuffer(perFrameBuffer, &perFrameConstants, sizeof(perFrameConstants));
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
