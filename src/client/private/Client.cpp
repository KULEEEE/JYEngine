#include "client/framework.h"
#include "client/Client.h"

#include "client/editor/JSceneManager.h"
#include "client/editor/JScenePanel.h"
#include "client/editor/JFBXLoader.h"

#include "engine/render/JRenderDefinition.h"
#include "engine/core/JEngineContext.h"

#include <iostream>
#include <shellapi.h>

J::Render::JWindowInfo s_WindowInfo;
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

			if (!arg.empty() && arg[0] != L'-' && options.scenePath.empty())
			{
				options.scenePath = arg;
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
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CLIENT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CLIENT));
    MSG msg;

    s_WindowInfo.width = 1920;
    s_WindowInfo.height = 1080;
    s_WindowInfo.windowed = true;
    RECT clientRect{};
    if (GetClientRect(s_WindowInfo.hwnd, &clientRect))
    {
        s_WindowInfo.width = static_cast<int>(clientRect.right - clientRect.left);
        s_WindowInfo.height = static_cast<int>(clientRect.bottom - clientRect.top);
    }

    J::Render::JSwapChain* swapChain = new J::Render::JSwapChain();
    J::Render::JCommandQueue* cmdQueue = new J::Render::JCommandQueue();

    InitializeEngine(cmdQueue, swapChain);

    cmdQueue->Initialize(GetEngine()->GetDevice()->GetDevice(), swapChain);
    swapChain->Initialize(s_WindowInfo, s_Engine->GetDevice(), cmdQueue->GetCmdQueue());

    const float initialAspectRatio = s_WindowInfo.height > 0
        ? static_cast<float>(s_WindowInfo.width) / static_cast<float>(s_WindowInfo.height)
        : 1.0f;
    {
        J::Editor::JSceneManager sceneManager(GetEngine()->GetRenderServer(), GetEngine()->GetMaterialFactory(), initialAspectRatio);
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

        if (!sceneManager.HasScene())
        {
            std::cerr << "No scene could be initialized." << std::endl;
            return FALSE;
        }

        unique_ptr<J::Editor::JEditorPanel> panel = make_unique<J::Editor::JScenePanel>(&sceneManager);
        panel->Init();
        s_ActiveEditorPanel = panel.get();

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

            cmdQueue->RenderBegin();
            panel->Update();
            cmdQueue->RenderEnd();
            swapChain->Present();
            cmdQueue->WaitSync();
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

    s_WindowInfo.hwnd = hWnd;
    RECT clientRect{};
    if (GetClientRect(hWnd, &clientRect))
    {
        s_WindowInfo.width = static_cast<int>(clientRect.right - clientRect.left);
        s_WindowInfo.height = static_cast<int>(clientRect.bottom - clientRect.top);
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
