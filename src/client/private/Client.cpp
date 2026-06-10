#include "client/framework.h"
#include "client/Client.h"

#include "client/editor/JSceneManager.h"
#include "client/editor/JScenePanel.h"
#include "client/editor/JImGuiLayer.h"
#include "client/editor/JFBXLoader.h"

#include "engine/render/JRenderDefinition.h"
#include "engine/core/JEngineContext.h"
#include "engine/render/JRenderer.h"

#include "imgui.h"

#include <iostream>
#include <algorithm>
#include <shellapi.h>

J::Editor::JEditorPanel* s_ActiveEditorPanel = nullptr;
J::Editor::JImGuiLayer* s_ActiveImGuiLayer = nullptr;

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

namespace
{
	void DrawRenderProfiler(J::Engine::JRenderer* renderer)
	{
		if (renderer == nullptr)
		{
			return;
		}

		const std::vector<J::Engine::JRenderer::PassTiming>& timings = renderer->GetLastFrameTimings();

		static std::vector<double> accumMs;     // 현재 구간 ms 합
		static std::vector<double> displayMs;   // 확정된 표시값(1초마다 갱신)
		static std::vector<uint32> displayDraws;
		static int   frameCount = 0;            // 현재 구간 프레임 수
		static int   displayFrameCount = 0;     // 확정된 구간 프레임 수(≈FPS)
		static double lastUpdateTime = 0.0;

		if (accumMs.size() != timings.size())
		{
			accumMs.assign(timings.size(), 0.0);
			displayMs.assign(timings.size(), 0.0);
			displayDraws.assign(timings.size(), 0);
			frameCount = 0;
		}

		for (size_t i = 0; i < timings.size(); ++i)
		{
			accumMs[i] += timings[i].cpuMs;
		}
		++frameCount;

		const double now = ImGui::GetTime();
		if (now - lastUpdateTime >= 1.0 && frameCount > 0)
		{
			for (size_t i = 0; i < timings.size(); ++i)
			{
				displayMs[i] = accumMs[i] / frameCount;
				displayDraws[i] = timings[i].drawCalls;
				accumMs[i] = 0.0;
			}
			displayFrameCount = frameCount;
			frameCount = 0;
			lastUpdateTime = now;
		}

		ImGui::Begin("Render Profiler");
#if defined(NDEBUG)
		ImGui::TextUnformatted("Build: optimized (Release/RelWithDebInfo)");
#else
		ImGui::TextUnformatted("Build: Debug - NOT for perf judgement");
#endif
		ImGui::Text("1s avg over %d frames (~%d fps)", displayFrameCount, displayFrameCount);
		ImGui::Separator();

		if (ImGui::BeginTable("pass_timings", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Pass");
			ImGui::TableSetupColumn("CPU ms");
			ImGui::TableSetupColumn("Draws");
			ImGui::TableHeadersRow();

			double totalMs = 0.0;
			uint32 totalDraws = 0;
			for (size_t i = 0; i < timings.size(); ++i)
			{
				totalMs += displayMs[i];
				totalDraws += displayDraws[i];

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(timings[i].name);
				ImGui::TableNextColumn();
				ImGui::Text("%.3f", displayMs[i]);
				ImGui::TableNextColumn();
				ImGui::Text("%u", displayDraws[i]);
			}

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("TOTAL");
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", totalMs);
			ImGui::TableNextColumn();
			ImGui::Text("%u", totalDraws);

			ImGui::EndTable();
		}
		ImGui::End();
	}

	struct SceneLaunchOptions
	{
		bool createNew = false;
		std::filesystem::path scenePath;
		std::filesystem::path projectPath;
	};

	SceneLaunchOptions parseSceneLaunchOptions()
	{
		SceneLaunchOptions options;
		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		if (argv == nullptr)
		{
			return options;
		}

		for (int i = 1; i < argc; ++i)
		{
			const std::wstring arg = argv[i];
			if (arg == L"--new" || arg == L"-n")
			{
				options.createNew = true;
				options.scenePath.clear();
				break;
			}

			if ((arg == L"--scene" || arg == L"-s") && i + 1 < argc)
			{
				options.scenePath = argv[++i];
				continue;
			}

			if ((arg == L"--project" || arg == L"-p") && i + 1 < argc)
			{
				options.projectPath = argv[++i];
				options.scenePath.clear();
				continue;
			}

			if (!arg.empty() && arg[0] != L'-' && options.scenePath.empty() && options.projectPath.empty())
			{
				options.projectPath = arg;
			}
		}

		LocalFree(argv);
		return options;
	}

	std::filesystem::path resolveScenePath(const std::filesystem::path& scenePath)
	{
		if (scenePath.empty())
		{
			return scenePath;
		}

		if (scenePath.is_absolute() || std::filesystem::exists(scenePath))
		{
			return scenePath;
		}

		const std::filesystem::path engineResPath = std::filesystem::path(get_Engine_Res_Path());
		const std::filesystem::path resolvedPath = engineResPath / scenePath;
		return std::filesystem::exists(resolvedPath) ? resolvedPath : scenePath;
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
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

#ifdef _DEBUG
    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CLIENT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CLIENT));
    MSG msg;

    J::Render::JWindowInfo& windowInfo = GetEngineWindowInfo();
    windowInfo.width = 1920;
    windowInfo.height = 1080;
    windowInfo.windowed = true;
    RECT clientRect{};
    if (GetClientRect(windowInfo.hwnd, &clientRect))
    {
        windowInfo.width = static_cast<int>(clientRect.right - clientRect.left);
        windowInfo.height = static_cast<int>(clientRect.bottom - clientRect.top);
    }

    auto swapChain = std::make_unique<J::Render::JSwapChain>();
    auto cmdQueue = std::make_unique<J::Render::JCommandQueue>();

    InitializeEngine(cmdQueue.get(), swapChain.get());

    cmdQueue->Initialize(GetEngine()->GetDevice()->GetDevice(), swapChain.get());
    swapChain->Initialize(windowInfo, s_Engine->GetDevice(), cmdQueue->GetCmdQueue());

    const float initialAspectRatio = windowInfo.height > 0
        ? static_cast<float>(windowInfo.width) / static_cast<float>(windowInfo.height)
        : 1.0f;
    {
        J::Editor::JSceneManager sceneManager(GetEngine()->GetRenderer(), GetEngine()->GetMaterialFactory(), initialAspectRatio);
        const SceneLaunchOptions launchOptions = parseSceneLaunchOptions();
        if (launchOptions.createNew)
        {
            if (!sceneManager.New())
            {
                std::cerr << "Failed to create a new scene." << std::endl;
            }
        }
        else
        {
            if (!launchOptions.projectPath.empty())
            {
                if (!sceneManager.OpenProject(launchOptions.projectPath))
                {
                    std::cerr << "Failed to open project: " << launchOptions.projectPath.string() << ". Falling back to a new scene." << std::endl;
                    sceneManager.New();
                }
            }
            else
            {
                std::filesystem::path scenePath = launchOptions.scenePath;
                if (scenePath.empty())
                {
                    scenePath = std::filesystem::path(get_Engine_Res_Path()) / "scene" / "sample.jscene.json";
                }
                else
                {
                    scenePath = resolveScenePath(scenePath);
                }

                if (!sceneManager.Open(scenePath))
                {
                    std::cerr << "Failed to open scene: " << scenePath.string() << ". Falling back to a new scene." << std::endl;
                    sceneManager.New();
                }
            }
        }

        if (!sceneManager.HasScene())
        {
            std::cerr << "No scene could be initialized." << std::endl;
            return FALSE;
        }

        unique_ptr<J::Editor::JScenePanel> panel = make_unique<J::Editor::JScenePanel>();
        panel->Init();
        s_ActiveEditorPanel = panel.get();

        // 에디터 UI 자원은 client/editor 레이어에서만 관리함
        J::Editor::JImGuiLayer imguiLayer;
        imguiLayer.Init(
            windowInfo.hwnd,
            GetEngine()->GetDevice()->GetDevice().Get(),
            cmdQueue->GetCmdQueue().Get(),
            DXGI_FORMAT_R8G8B8A8_UNORM);
        s_ActiveImGuiLayer = &imguiLayer;
        uint32 swapChainWidth = static_cast<uint32>(std::max(windowInfo.width, 1));
        uint32 swapChainHeight = static_cast<uint32>(std::max(windowInfo.height, 1));

        while (true)
        {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                    break;

                if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }

            uint32 clientWidth = 1;
            uint32 clientHeight = 1;
            getClientSize(windowInfo.hwnd, clientWidth, clientHeight);
            if (clientWidth != swapChainWidth || clientHeight != swapChainHeight)
            {
                cmdQueue->WaitIdle();
                swapChain->Resize(clientWidth, clientHeight);
                swapChainWidth = clientWidth;
                swapChainHeight = clientHeight;
                windowInfo.width = static_cast<int32>(clientWidth);
                windowInfo.height = static_cast<int32>(clientHeight);
            }

            const uint32 frameIndex = swapChain->GetBackBufferIndex();
            panel->SetRenderTarget(swapChain->GetRenderTarget());

            cmdQueue->RenderBegin(frameIndex);
            J::Engine::JScene* scene = sceneManager.GetScene();
            if (scene != nullptr)
            {
                panel->Update(*scene);
            }
            if (imguiLayer.IsInitialized())
            {
                // ImGui draw command를 만들기 전 frame 상태부터 열어야 함
                imguiLayer.BeginFrame();
                if (scene != nullptr)
                {
                    panel->DrawEditorUI(*scene);
                }
#ifdef _DEBUG
                panel->DrawStatsUI();
#endif
                DrawRenderProfiler(GetEngine()->GetRenderer());
            }
            if (scene != nullptr && panel->CanRender())
            {
#ifdef _DEBUG
                J::Engine::JRenderer::FrameDesc frameDesc;
                if (GetEngine()->RenderScene(
                    *scene,
                    panel->GetRenderTarget(),
                    &frameDesc))
                {
                    panel->OnSceneRendered(frameDesc);
                }
#else
                if (GetEngine()->RenderScene(*scene, panel->GetRenderTarget()))
                {
                }
#endif
                else
                {
#ifdef _DEBUG
                    std::cerr << "Frame skipped: failed to render scene." << std::endl;
#endif
                }
            }
            if (imguiLayer.IsInitialized() && panel->CanRender())
            {
                // Scene 렌더가 끝난 뒤 같은 back buffer 위에 editor UI를 올림
                imguiLayer.Render(panel->GetRenderTarget(), cmdQueue.get());
            }
            cmdQueue->RenderEnd(frameIndex);
            swapChain->Present();
            swapChain->SwapIndex();
        }

        s_ActiveImGuiLayer = nullptr;
        imguiLayer.Shutdown();
        panel.reset();
        s_ActiveEditorPanel = nullptr;
    }

    DestroyEngine();
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CLIENT));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    RECT windowRect = { 0, 0, 1920, 1080 };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hWnd = CreateWindowW(
        szWindowClass,
        szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        0,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    J::Render::JWindowInfo& windowInfo = GetEngineWindowInfo();
    windowInfo.hwnd = hWnd;
    RECT clientRect{};
    if (GetClientRect(hWnd, &clientRect))
    {
        windowInfo.width = static_cast<int>(clientRect.right - clientRect.left);
        windowInfo.height = static_cast<int>(clientRect.bottom - clientRect.top);
    }

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (s_ActiveImGuiLayer != nullptr && s_ActiveImGuiLayer->IsInitialized())
    {
        // ImGui가 먹은 입력이면 기본 Win32 처리로 넘기지 않음
        if (J::Editor::JImGuiLayer::HandleWin32Message(hWnd, message, wParam, lParam))
        {
            return TRUE;
        }
    }

    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            UNREFERENCED_PARAMETER(hdc);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_MOUSEWHEEL:
        if (s_ActiveEditorPanel != nullptr)
        {
            s_ActiveEditorPanel->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
