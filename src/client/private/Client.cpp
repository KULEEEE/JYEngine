#include "client/framework.h"
#include "client/Client.h"

#include "client/editor/JSceneManager.h"
#include "client/editor/JScenePanel.h"
#include "client/editor/JFBXLoader.h"

#include "engine/render/JRenderDefinition.h"
#include "engine/core/JEngineContext.h"

#include <iostream>
#include <algorithm>
#include <shellapi.h>

J::Editor::JEditorPanel* s_ActiveEditorPanel = nullptr;

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
            cmdQueue->RenderEnd(frameIndex);
            swapChain->Present();
            swapChain->SwapIndex();
        }

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
