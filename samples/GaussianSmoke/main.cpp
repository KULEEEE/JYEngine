#include "engine/core/JEngineContext.h"
#include "engine/asset/JMaterial.h"
#include "engine/asset/JMesh.h"
#include "engine/render/JMaterialFactory.h"
#include "engine/render/JCommandQueue.h"
#include "engine/render/JSwapChain.h"
#include "engine/scene/JScene.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <shellapi.h>
#include <vector>

namespace
{
	const wchar_t* SAMPLE_WINDOW_CLASS = L"JYEngineGaussianSmokeSampleWindow";
	const wchar_t* STATS_WINDOW_CLASS = L"JYEngineGaussianSmokeStatsWindow";
	const wchar_t* SAMPLE_WINDOW_TITLE = L"JYEngine - Experimental Gaussian Smoke Sample";

	bool g_isRunning = true;

	struct SampleConfig
	{
		uint32 particleCount = 256;
		uint32 updateCount = 256;
	};

	struct StatsWindow
	{
		HWND window = nullptr;
		HWND text = nullptr;
	};

	struct SmokeParticle
	{
		J::Engine::JTransformHandle transform = {};
		JVec3 basePosition = {};
		float driftPhase = 0.0f;
		float driftRadius = 0.0f;
		float driftSpeed = 0.0f;
	};

	struct SmokeSimulation
	{
		std::vector<SmokeParticle> particles;
	};

	uint32 parsePositiveUInt(const wchar_t* value, uint32 fallback)
	{
		if (value == nullptr || value[0] == L'\0')
		{
			return fallback;
		}

		wchar_t* end = nullptr;
		const unsigned long parsed = wcstoul(value, &end, 10);
		return end != value && parsed > 0 ? static_cast<uint32>(parsed) : fallback;
	}

	SampleConfig parseSampleConfig()
	{
		SampleConfig config;

		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if (argv == nullptr)
		{
			return config;
		}

		for (int i = 1; i < argc; ++i)
		{
			const std::wstring arg = argv[i];
			if ((arg == L"--particles" || arg == L"-p") && i + 1 < argc)
			{
				config.particleCount = parsePositiveUInt(argv[++i], config.particleCount);
				continue;
			}

			if ((arg == L"--update-count" || arg == L"-u") && i + 1 < argc)
			{
				config.updateCount = parsePositiveUInt(argv[++i], config.updateCount);
				continue;
			}
		}

		LocalFree(argv);

		constexpr uint32 MAX_PARTICLE_COUNT = 4096;
		config.particleCount = std::max(1u, std::min(config.particleCount, MAX_PARTICLE_COUNT));
		config.updateCount = std::min(config.updateCount, config.particleCount);
		return config;
	}

	void getClientSize(HWND hwnd, uint32& outWidth, uint32& outHeight)
	{
		RECT rect{};
		if (hwnd != nullptr && GetClientRect(hwnd, &rect))
		{
			outWidth = static_cast<uint32>(std::max<LONG>(rect.right - rect.left, 1));
			outHeight = static_cast<uint32>(std::max<LONG>(rect.bottom - rect.top, 1));
			return;
		}

		outWidth = 1;
		outHeight = 1;
	}

	LRESULT CALLBACK sampleWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_CLOSE:
			g_isRunning = false;
			DestroyWindow(hwnd);
			return 0;

		case WM_DESTROY:
			g_isRunning = false;
			PostQuitMessage(0);
			return 0;

		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
		}
	}

	LRESULT CALLBACK statsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_CLOSE:
			ShowWindow(hwnd, SW_HIDE);
			return 0;

		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
		}
	}

	bool registerSampleWindowClass(HINSTANCE instance)
	{
		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = sampleWndProc;
		wc.hInstance = instance;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wc.lpszClassName = SAMPLE_WINDOW_CLASS;

		return RegisterClassExW(&wc) != 0;
	}

	bool registerStatsWindowClass(HINSTANCE instance)
	{
		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = statsWndProc;
		wc.hInstance = instance;
		wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wc.lpszClassName = STATS_WINDOW_CLASS;

		return RegisterClassExW(&wc) != 0;
	}

	HWND createSampleWindow(HINSTANCE instance, int showCommand)
	{
		constexpr int initialWidth = 1280;
		constexpr int initialHeight = 720;

		RECT windowRect{ 0, 0, initialWidth, initialHeight };
		AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

		const int windowWidth = windowRect.right - windowRect.left;
		const int windowHeight = windowRect.bottom - windowRect.top;

		HWND hwnd = CreateWindowW(
			SAMPLE_WINDOW_CLASS,
			SAMPLE_WINDOW_TITLE,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			windowWidth,
			windowHeight,
			nullptr,
			nullptr,
			instance,
			nullptr);

		if (hwnd == nullptr)
		{
			return nullptr;
		}

		ShowWindow(hwnd, showCommand);
		UpdateWindow(hwnd);
		return hwnd;
	}

	StatsWindow createStatsWindow(HINSTANCE instance, HWND owner)
	{
		StatsWindow stats;
		stats.window = CreateWindowW(
			STATS_WINDOW_CLASS,
			L"Gaussian Smoke Stats",
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			420,
			150,
			owner,
			nullptr,
			instance,
			nullptr);

		if (stats.window == nullptr)
		{
			return stats;
		}

		stats.text = CreateWindowW(
			L"STATIC",
			L"",
			WS_CHILD | WS_VISIBLE | SS_LEFT,
			12,
			12,
			380,
			90,
			stats.window,
			nullptr,
			instance,
			nullptr);

		ShowWindow(stats.window, SW_SHOW);
		UpdateWindow(stats.window);
		return stats;
	}

	void updateStatsWindow(
		const StatsWindow& stats,
		const SampleConfig& config,
		double fps,
		const J::Engine::JRenderer::FrameDesc& frameDesc)
	{
		if (stats.text == nullptr)
		{
			return;
		}

		const uint32 opaqueDraws = static_cast<uint32>(frameDesc.opaqueDrawItemIndices.size());
		const uint32 transparentDraws = static_cast<uint32>(frameDesc.transparentDrawItemIndices.size());
		const uint32 drawCalls = opaqueDraws + transparentDraws;
		const uint32 dirtyTransforms = static_cast<uint32>(frameDesc.transformSnapshots.size());

		wchar_t text[512] = {};
		swprintf_s(
			text,
			L"FPS: %.1f\r\nDrawCalls: %u  (opaque %u / transparent %u)\r\nParticles: %u\r\nUpdated Particles: %u\r\nDirty Transforms: %u",
			fps,
			drawCalls,
			opaqueDraws,
			transparentDraws,
			config.particleCount,
			config.updateCount,
			dirtyTransforms);

		SetWindowTextW(stats.text, text);
	}

	void openDebugConsole()
	{
#ifdef _DEBUG
		AllocConsole();
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
#endif
	}

	std::string getGaussianBillboardShaderPath()
	{
		const std::filesystem::path sourceRoot = JY_ENGINE_SOURCE_DIR;
		return (sourceRoot / "samples" / "GaussianSmoke" / "shader" / "gaussian_billboard.hlsl").string();
	}

	std::shared_ptr<J::Engine::JMesh> createBillboardQuadMesh()
	{
		using namespace J::Engine;

		auto mesh = std::make_shared<JMesh>();

		// JRenderContext currently creates a position/normal/texcoord input
		// layout for regular material draws, so the sample quad provides all
		// three streams even though the smoke shader only needs position/uv.
		mesh->SetPositions({
			-1.0f, -1.0f, 0.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f,
			 1.0f,  1.0f, 0.0f, 1.0f,
			-1.0f,  1.0f, 0.0f, 1.0f,
		});

		mesh->SetNormals({
			0.0f, 0.0f, -1.0f,
			0.0f, 0.0f, -1.0f,
			0.0f, 0.0f, -1.0f,
			0.0f, 0.0f, -1.0f,
		});

		mesh->SetTexcoords({
			0.0f, 1.0f,
			1.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 0.0f,
		}, 0);

		mesh->SetIndices({
			0, 1, 2,
			0, 2, 3,
		});

		mesh->SetSubMeshes({
			{ 0, 0, 6 },
		});

		return mesh;
	}

	J::Engine::JMaterialHandle createSmokeMaterial(J::Engine::JScene& scene)
	{
		using namespace J::Engine;

		JMaterialFactory* materialFactory = GetEngine()->GetMaterialFactory();
		if (materialFactory == nullptr)
		{
			return {};
		}

		struct SmokeMaterialConstants
		{
			JVec4 smokeColor = { 0.78f, 0.84f, 0.90f, 0.10f };
			float sharpness = 4.25f;
			JVec3 padding = {};
		};

		std::shared_ptr<JMaterial> material(
			materialFactory->CreateMaterial(getGaussianBillboardShaderPath(), true));
		if (material == nullptr)
		{
			return {};
		}

		material->SetName("Gaussian Billboard Smoke");

		SmokeMaterialConstants constants{};
		materialFactory->SetConstantBufferData(
			material.get(),
			"PerMaterial",
			&constants,
			sizeof(constants));

		return scene.AddMaterial(material);
	}

	void addDynamicSmokeBillboards(
		J::Engine::JScene& scene,
		J::Engine::JMaterialHandle smokeMaterial,
		const std::shared_ptr<J::Engine::JMesh>& quadMesh,
		SmokeSimulation& simulation,
		uint32 particleCount)
	{
		using namespace J::Engine;

		if (!smokeMaterial.IsValid() || quadMesh == nullptr)
		{
			return;
		}

		simulation.particles.clear();
		simulation.particles.reserve(particleCount);

		const uint32 columnCount = std::max(1u, static_cast<uint32>(std::ceil(std::sqrt(static_cast<float>(particleCount) * 4.0f))));
		const uint32 rowCount = std::max(1u, (particleCount + columnCount - 1) / columnCount);

		for (uint32 i = 0; i < particleCount; ++i)
		{
			const std::string id = "sample_smoke_billboard_" + std::to_string(i);
			const std::string name = "Dynamic Smoke Billboard " + std::to_string(i);

			JEntityHandle entity = scene.CreateEntity(id, name);

			const float t = static_cast<float>(i);
			const float row = static_cast<float>(i / columnCount);
			const float column = static_cast<float>(i % columnCount);
			const float halfColumn = std::max((static_cast<float>(columnCount) - 1.0f) * 0.5f, 1.0f);
			const float halfRow = std::max((static_cast<float>(rowCount) - 1.0f) * 0.5f, 1.0f);
			const float normalizedX = (column - halfColumn) / halfColumn;
			const float normalizedY = (row - halfRow) / halfRow;

			const float layer = std::fmod(t * 0.37f, 1.0f);
			const float cloudX = normalizedX * 2.7f + std::sinf(t * 1.73f) * 0.18f;
			const float cloudY = normalizedY * 0.65f + std::cosf(t * 1.11f) * 0.18f;
			const float cloudZ = (layer - 0.5f) * 0.45f;
			const float scale = 0.28f + 0.18f * (0.5f + 0.5f * std::sinf(t * 0.91f));

			JTransformComponents transform{};
			transform.translation = { cloudX, cloudY, cloudZ };
			transform.scale = { scale, scale, scale };

			JTransformHandle transformHandle = scene.AddTransform(entity, transform);
			scene.AddRenderObjectComponent(entity, smokeMaterial, quadMesh.get(), true);

			SmokeParticle particle{};
			particle.transform = transformHandle;
			particle.basePosition = transform.translation;
			particle.driftPhase = t * 0.43f;
			particle.driftRadius = 0.16f + 0.18f * (0.5f + 0.5f * std::cosf(t * 0.77f));
			particle.driftSpeed = 1.35f + 0.95f * (0.5f + 0.5f * std::sinf(t * 0.29f));
			simulation.particles.push_back(particle);
		}
	}

	void updateSmokeSimulation(
		J::Engine::JScene& scene,
		SmokeSimulation& simulation,
		float elapsedSeconds,
		uint32 updateCount)
	{
		using namespace J::Engine;

		// This intentionally updates many ordinary scene transforms.  It is a
		// baseline stress path for the existing dirty transform consumption and
		// RenderDB transform sync before we introduce instanced dynamic buffers.
		const uint32 activeUpdateCount = std::min(updateCount, static_cast<uint32>(simulation.particles.size()));
		for (uint32 particleIndex = 0; particleIndex < activeUpdateCount; ++particleIndex)
		{
			SmokeParticle& particle = simulation.particles[particleIndex];
			const float phase = elapsedSeconds * particle.driftSpeed + particle.driftPhase;

			JVec3 position = particle.basePosition;
			position.x += std::sinf(phase) * particle.driftRadius;
			position.y += std::cosf(phase * 0.73f) * particle.driftRadius * 0.85f;
			position.z += std::sinf(phase * 1.31f) * particle.driftRadius * 0.75f;

			scene.SetTransformTranslation(particle.transform, position);
		}
	}

	void buildMinimalScene(
		J::Engine::JScene& scene,
		float aspectRatio,
		const std::shared_ptr<J::Engine::JMesh>& smokeQuadMesh,
		SmokeSimulation& simulation,
		uint32 particleCount)
	{
		using namespace J::Engine;

		// This sample keeps scene authoring in code on purpose.  The goal of
		// Task 1 is to prove the engine runtime path works without editor UI,
		// project loading, or ScenePanel state.
		JEntityHandle cameraEntity = scene.CreateEntity("sample_camera", "Sample Camera");
		JTransformComponents cameraTransform{};
		cameraTransform.translation = { 0.0f, 1.5f, -8.0f };
		cameraTransform.rotation = { 0.0f, 0.0f, 0.0f };
		cameraTransform.scale = { 1.0f, 1.0f, 1.0f };

		JTransformHandle cameraTransformHandle = scene.AddTransform(cameraEntity, cameraTransform);
		JCameraHandle camera = scene.AddCamera(cameraEntity, cameraTransformHandle, aspectRatio, 0.1f, 1000.0f);
		scene.SetPrimaryCamera(camera);

		JEntityHandle lightEntity = scene.CreateEntity("sample_light", "Sample Light");
		JTransformComponents lightTransform{};
		lightTransform.translation = { 0.0f, 4.0f, -4.0f };
		lightTransform.scale = { 1.0f, 1.0f, 1.0f };

		scene.AddTransform(lightEntity, lightTransform);

		JLightComponents light{};
		light.color = { 1.0f, 0.95f, 0.85f };
		light.intensity = 1.0f;
		scene.AddLight(lightEntity, light);

		JMaterialHandle smokeMaterial = createSmokeMaterial(scene);
		addDynamicSmokeBillboards(scene, smokeMaterial, smokeQuadMesh, simulation, particleCount);
	}

	void pumpWindowMessages()
	{
		MSG msg{};
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				g_isRunning = false;
				return;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

int APIENTRY wWinMain(
	_In_ HINSTANCE instance,
	_In_opt_ HINSTANCE previousInstance,
	_In_ LPWSTR commandLine,
	_In_ int showCommand)
{
	UNREFERENCED_PARAMETER(previousInstance);
	UNREFERENCED_PARAMETER(commandLine);

	openDebugConsole();
	const SampleConfig config = parseSampleConfig();

	if (!registerSampleWindowClass(instance))
	{
		std::cerr << "Failed to register GaussianSmoke sample window class." << std::endl;
		return FALSE;
	}

	if (!registerStatsWindowClass(instance))
	{
		std::cerr << "Failed to register GaussianSmoke stats window class." << std::endl;
		return FALSE;
	}

	HWND hwnd = createSampleWindow(instance, showCommand);
	if (hwnd == nullptr)
	{
		std::cerr << "Failed to create GaussianSmoke sample window." << std::endl;
		return FALSE;
	}

	StatsWindow statsWindow = createStatsWindow(instance, hwnd);

	J::Render::JWindowInfo& windowInfo = GetEngineWindowInfo();
	windowInfo.hwnd = hwnd;
	windowInfo.windowed = true;

	uint32 initialClientWidth = 1;
	uint32 initialClientHeight = 1;
	getClientSize(hwnd, initialClientWidth, initialClientHeight);
	windowInfo.width = static_cast<int32>(initialClientWidth);
	windowInfo.height = static_cast<int32>(initialClientHeight);

	auto swapChain = std::make_unique<J::Render::JSwapChain>();
	auto commandQueue = std::make_unique<J::Render::JCommandQueue>();

	InitializeEngine(commandQueue.get(), swapChain.get());
	commandQueue->Initialize(GetEngine()->GetDevice()->GetDevice(), swapChain.get());
	swapChain->Initialize(windowInfo, GetEngine()->GetDevice(), commandQueue->GetCmdQueue());

	// The smoke quads are transparent overlay content.  Deferred mode gives
	// that path a depth target through JForwardOverlayPass, while keeping this
	// sample out of ScenePanel/editor code.
	GetEngine()->GetRenderer()->SetRenderPath(J::Engine::JRenderer::RenderPath::Deferred);

	const float aspectRatio = windowInfo.height > 0
		? static_cast<float>(windowInfo.width) / static_cast<float>(windowInfo.height)
		: 1.0f;

	J::Engine::JScene scene;
	std::shared_ptr<J::Engine::JMesh> smokeQuadMesh = createBillboardQuadMesh();
	SmokeSimulation simulation;
	buildMinimalScene(scene, aspectRatio, smokeQuadMesh, simulation, config.particleCount);

	const auto startTime = std::chrono::steady_clock::now();
	auto lastStatsUpdateTime = startTime;
	uint32 framesSinceStatsUpdate = 0;

	uint32 swapChainWidth = static_cast<uint32>(std::max(windowInfo.width, 1));
	uint32 swapChainHeight = static_cast<uint32>(std::max(windowInfo.height, 1));

	while (g_isRunning)
	{
		pumpWindowMessages();
		if (!g_isRunning)
		{
			break;
		}

		uint32 clientWidth = 1;
		uint32 clientHeight = 1;
		getClientSize(hwnd, clientWidth, clientHeight);
		if (clientWidth != swapChainWidth || clientHeight != swapChainHeight)
		{
			commandQueue->WaitIdle();
			swapChain->Resize(clientWidth, clientHeight);
			swapChainWidth = clientWidth;
			swapChainHeight = clientHeight;
			windowInfo.width = static_cast<int32>(clientWidth);
			windowInfo.height = static_cast<int32>(clientHeight);
			scene.SetCameraAspectRatio(scene.GetPrimaryCamera(), static_cast<float>(clientWidth) / static_cast<float>(clientHeight));
		}

		const auto frameStartTime = std::chrono::steady_clock::now();
		const float elapsedSeconds = std::chrono::duration<float>(frameStartTime - startTime).count();
		updateSmokeSimulation(scene, simulation, elapsedSeconds, config.updateCount);

		const uint32 frameIndex = swapChain->GetBackBufferIndex();
		commandQueue->RenderBegin(frameIndex);

		// No ScenePanel, no ImGui, no project loader: the sample calls the
		// same engine render entry point that the editor eventually uses.
		J::Engine::JRenderer::FrameDesc frameDesc;
		if (!GetEngine()->RenderScene(scene, swapChain->GetRenderTarget(), &frameDesc))
		{
#ifdef _DEBUG
			std::cerr << "GaussianSmoke sample frame skipped: RenderScene failed." << std::endl;
#endif
		}

		commandQueue->RenderEnd(frameIndex);
		swapChain->Present();
		swapChain->SwapIndex();

		++framesSinceStatsUpdate;
		const auto frameEndTime = std::chrono::steady_clock::now();
		const double statsElapsedSeconds = std::chrono::duration<double>(frameEndTime - lastStatsUpdateTime).count();
		if (statsElapsedSeconds >= 0.25)
		{
			const double fps = static_cast<double>(framesSinceStatsUpdate) / statsElapsedSeconds;
			updateStatsWindow(statsWindow, config, fps, frameDesc);

			framesSinceStatsUpdate = 0;
			lastStatsUpdateTime = frameEndTime;
		}
	}

	commandQueue->WaitIdle();
	DestroyEngine();
	return 0;
}
