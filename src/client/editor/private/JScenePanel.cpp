#include "client/editor/JScenePanel.h"

#include "engine/core/JEngineContext.h"
#include "engine/render/JRenderer.h"
#include "engine/render/JRenderTarget.h"
#include "engine/render/JMaterialFactory.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"

#include "imgui.h"

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

	bool hasTag(const Engine::JEntityMetadata& metadata, const std::string& tag)
	{
		return std::find(metadata.tags.begin(), metadata.tags.end(), tag) != metadata.tags.end();
	}

	void drawComponentBullet(const char* name)
	{
		ImGui::BulletText("%s", name);
	}

	// 첫 번째 active directional light를 전역 라이트로 간주하고 각도/세기를 편집함.
	// 라이트 스냅샷은 매 프레임 scene에서 다시 읽히므로 값만 바꾸면 즉시 반영됨.
	void drawGlobalLightControls(Engine::JScene& scene)
	{
		const std::vector<Engine::JScene::LightSlot>& lightSlots = scene.GetLightSlots();
		for (uint32 index = 0; index < static_cast<uint32>(lightSlots.size()); ++index)
		{
			const Engine::JScene::LightSlot& slot = lightSlots[index];
			if (!slot.active || !slot.data.active || slot.data.type != Engine::JLightType::Directional)
			{
				continue;
			}

			const Engine::JLightHandle lightHandle = { index, slot.generation };
			const JVec3* rotation = scene.GetTransformRotation(slot.data.entity);
			if (rotation == nullptr)
			{
				break;
			}

			ImGui::TextUnformatted("Global Light");
			ImGui::Separator();

			constexpr float radToDeg = 180.0f / 3.14159265f;
			constexpr float degToRad = 3.14159265f / 180.0f;
			float rotationDegrees[3] = { rotation->x * radToDeg, rotation->y * radToDeg, rotation->z * radToDeg };
			if (ImGui::DragFloat3("Rotation", rotationDegrees, 0.5f, -360.0f, 360.0f, "%.1f deg"))
			{
				scene.SetTransformRotation(slot.data.entity, { rotationDegrees[0] * degToRad, rotationDegrees[1] * degToRad, rotationDegrees[2] * degToRad });
			}

			Engine::JScene::LightData lightData = slot.data;
			if (ImGui::DragFloat("Intensity", &lightData.intensity, 0.05f, 0.0f, 100.0f, "%.2f"))
			{
				scene.SetLightData(lightHandle, lightData);
			}

			ImGui::Spacing();
			break;
		}
	}
}

JScenePanel::~JScenePanel()
{
}

bool JScenePanel::CanRender() const
{
	return _renderTarget != nullptr;
}

void JScenePanel::Init()
{
	Engine::JRenderer* renderer = GetEngine()->GetRenderer();
	if (renderer != nullptr)
	{
		renderer->SetRenderPath(Engine::JRenderer::RenderPath::Deferred);
	}

	QueryPerformanceFrequency(&_timerFrequency);
	QueryPerformanceCounter(&_lastFrameCounter);
}

void JScenePanel::Update()
{
}

void JScenePanel::Update(Engine::JScene& scene)
{
	const Engine::JCameraHandle sceneCamera = scene.GetPrimaryCamera();
	if (!sceneCamera.IsValid() || scene.GetCamera(sceneCamera) == nullptr)
	{
#ifdef _DEBUG
		std::cerr << "JScenePanel::Update skipped: scene camera is not ready." << std::endl;
#endif
		return;
	}

	createEditorGrid(scene);

	const HWND mainWindow = GetEngineWindowHandle();
	const uint32 clientWidth = getClientWidth(mainWindow);
	const uint32 clientHeight = getClientHeight(mainWindow);
	scene.SetCameraAspectRatio(sceneCamera, static_cast<float>(clientWidth) / static_cast<float>(clientHeight));

	const float frameTime = tickFrameTimer();
	_lastFrameTime = frameTime;
	_lastMovementDeltaTime = std::clamp(frameTime, 0.0f, MAX_FRAME_DELTA_TIME);

	updateSceneCamera(scene, sceneCamera, _lastMovementDeltaTime);
	if (mainWindow != nullptr && GetForegroundWindow() == mainWindow && (GetAsyncKeyState(VK_F2) & 0x0001))
	{
		_showEditorUI = !_showEditorUI;
	}
	if (mainWindow != nullptr && GetForegroundWindow() == mainWindow && (GetAsyncKeyState(VK_F1) & 0x0001))
	{
		_showStatsPopup = !_showStatsPopup;
	}

	if (_renderTarget != nullptr)
	{
		_renderTarget->SetClearColor(JColors::DarkGray);
		_renderTarget->SetRenderArea(clientWidth, clientHeight);
	}
}

void JScenePanel::DrawEditorUI(Engine::JScene& scene)
{
	if (!_showEditorUI)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(380.0f, 560.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Scene", &_showEditorUI))
	{
		ImGui::End();
		return;
	}

	drawGlobalLightControls(scene);

	ImGui::TextUnformatted("Hierarchy");
	ImGui::Separator();

	// Scene 공개 API만 읽어서 hierarchy 구성함
	uint32 visibleEntityCount = 0;
	const std::vector<Engine::JScene::EntitySlot>& entitySlots = scene.GetEntitySlots();
	for (uint32 index = 0; index < entitySlots.size(); ++index)
	{
		const Engine::JScene::EntitySlot& slot = entitySlots[index];
		if (!slot.active)
		{
			continue;
		}

		const Engine::JEntityHandle entity = { index, slot.generation };
		const Engine::JEntityMetadata* metadata = scene.GetEntityMetadata(entity);
		// 에디터가 만든 grid 같은 내부 entity는 표시 안 함
		if (metadata != nullptr && hasTag(*metadata, "editor_only"))
		{
			continue;
		}

		++visibleEntityCount;
		const char* entityName = metadata != nullptr && !metadata->name.empty()
			? metadata->name.c_str()
			: "Unnamed Entity";
		const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(index)), flags, "%s  #%u", entityName, index))
		{
			if (metadata != nullptr && !metadata->stableID.empty())
			{
				ImGui::TextDisabled("id: %s", metadata->stableID.c_str());
			}

			ImGui::TextUnformatted("Components");
			ImGui::Indent();
			// 현재 Scene이 제공하는 컴포넌트 핸들 유효성으로 보유 여부 판단함
			bool hasComponent = false;
			if (scene.GetTransformHandle(entity).IsValid())
			{
				drawComponentBullet("Transform");
				hasComponent = true;
			}
			if (scene.GetCameraHandle(entity).IsValid())
			{
				drawComponentBullet("Camera");
				hasComponent = true;
			}
			if (scene.GetLightHandle(entity).IsValid())
			{
				drawComponentBullet("Light");
				hasComponent = true;
			}
			if (scene.GetRenderObjectComponentHandle(entity).IsValid())
			{
				drawComponentBullet("RenderObject");
				hasComponent = true;
			}
			if (!hasComponent)
			{
				ImGui::TextDisabled("None");
			}
			ImGui::Unindent();
			ImGui::TreePop();
		}
	}

	if (visibleEntityCount == 0)
	{
		ImGui::TextDisabled("No scene entities.");
	}

	ImGui::End();
}

void JScenePanel::DrawStatsUI()
{
	if (!_showStatsPopup)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(220.0f, 110.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(20.0f, 600.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Frame", &_showStatsPopup, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("FPS: %.1f", _displayFps);
	ImGui::Text("Frame: %.2f ms", _displayFrameMs);
	ImGui::Text("DrawCalls: %u", _displayDrawCallCount);
	ImGui::End();
}

void JScenePanel::OnSceneRendered(const Engine::JFrameDesc& frameDesc)
{
	updateStatsData(frameDesc);
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
	return static_cast<float>(elapsed);
}

void JScenePanel::OnMouseWheel(short delta)
{
	const HWND mainWindow = GetEngineWindowHandle();
	if (mainWindow == nullptr || GetForegroundWindow() != mainWindow)
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
}

void JScenePanel::createEditorGrid(Engine::JScene& scene)
{
	Engine::JEngine* engine = GetEngine();
	if (engine == nullptr)
	{
		return;
	}

	if (scene.FindEntityByStableID("__editor_grid").IsValid())
	{
		return;
	}

	if (_editorGridMaterial == nullptr)
	{
		Engine::JMaterialFactory* materialFactory = engine->GetMaterialFactory();
		if (materialFactory == nullptr)
		{
			return;
		}

		const std::string gridShaderPath = (std::filesystem::path(get_Engine_Res_Path()) / "shader" / "grid.hlsl").string();
		_editorGridMaterial = std::shared_ptr<Engine::JMaterial>(materialFactory->CreateMaterial(gridShaderPath, true));
		if (_editorGridMaterial == nullptr)
		{
			std::cerr << "JScenePanel::createEditorGrid failed: grid material creation failed." << std::endl;
			return;
		}

		_editorGridMaterialHandle = scene.AddMaterial(_editorGridMaterial);
		if (!_editorGridMaterialHandle.IsValid())
		{
			std::cerr << "JScenePanel::createEditorGrid failed: grid material registration failed." << std::endl;
			_editorGridMaterial.reset();
			return;
		}
	}
	else if (scene.GetMaterial(_editorGridMaterialHandle) == nullptr)
	{
		_editorGridMaterialHandle = scene.AddMaterial(_editorGridMaterial);
		if (!_editorGridMaterialHandle.IsValid())
		{
			std::cerr << "JScenePanel::createEditorGrid failed: grid material registration failed." << std::endl;
			return;
		}
	}

	if (_editorGridMesh == nullptr)
	{
		_editorGridMesh = std::make_unique<Engine::JMesh>();
		_editorGridMesh->SetPositions({
			-EDITOR_GRID_SIZE, EDITOR_GRID_Y, -EDITOR_GRID_SIZE, 1.0f,
			 EDITOR_GRID_SIZE, EDITOR_GRID_Y, -EDITOR_GRID_SIZE, 1.0f,
			 EDITOR_GRID_SIZE, EDITOR_GRID_Y,  EDITOR_GRID_SIZE, 1.0f,
			-EDITOR_GRID_SIZE, EDITOR_GRID_Y,  EDITOR_GRID_SIZE, 1.0f,
		});
		_editorGridMesh->SetIndices({ 0, 1, 2, 0, 2, 3 });
	}

	const Engine::JEntityHandle editorGridEntity = scene.CreateEntity("__editor_grid", "Editor Grid", { "editor_only" });
	if (!editorGridEntity.IsValid())
	{
		return;
	}

	Engine::JScene::TransformData transform{};
	transform.translation = { 0.0f, 0.0f, 0.0f };
	transform.rotation = { 0.0f, 0.0f, 0.0f };
	transform.scale = { 1.0f, 1.0f, 1.0f };
	scene.AddTransform(editorGridEntity, transform);

	scene.AddRenderObjectComponent(
		editorGridEntity,
		_editorGridMaterialHandle,
		_editorGridMesh.get(),
		true);
}

void JScenePanel::updateSceneCamera(Engine::JScene& scene, Engine::JCameraHandle sceneCamera, float deltaTime)
{
	if (!sceneCamera.IsValid())
	{
		return;
	}

	const HWND mainWindow = GetEngineWindowHandle();
	if (mainWindow == nullptr || GetForegroundWindow() != mainWindow)
	{
		_isMouseLookActive = false;
		return;
	}

	Engine::JScene::CameraData* cameraData = scene.GetCamera(sceneCamera);
	if (cameraData == nullptr)
	{
		return;
	}

	const Engine::JTransformHandle transformHandle = scene.GetTransformHandle(cameraData->entity);
	if (!transformHandle.IsValid())
	{
		return;
	}

	const Engine::JScene::TransformData transformData = scene.GetTransform(transformHandle);

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

	scene.SetTransformRotation(transformHandle, { newRotation.x, newRotation.y, newRotation.z });
	scene.SetTransformTranslation(transformHandle, { newPosition.x, newPosition.y, newPosition.z });
}

void JScenePanel::updateStatsData(const Engine::JFrameDesc& frameDesc)
{
	constexpr float STATS_UPDATE_INTERVAL = 0.25f;
	_statsElapsed += _lastFrameTime;
	++_statsFrameCount;

	if (_statsElapsed >= STATS_UPDATE_INTERVAL)
	{
		_displayFps = _statsElapsed > 0.0f ? static_cast<float>(_statsFrameCount) / _statsElapsed : 0.0f;
		_displayFrameMs = _displayFps > 0.0f ? 1000.0f / _displayFps : 0.0f;
		_displayDrawCallCount = static_cast<uint32>(frameDesc.opaqueDrawItemIndices.size() + frameDesc.transparentDrawItemIndices.size());
		_statsElapsed = 0.0f;
		_statsFrameCount = 0;
	}
}

J_EDITOR_END
