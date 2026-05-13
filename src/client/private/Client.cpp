// Client.cpp : ? í”Œë¦¬ì??´ì…˜???€??ì§„ì…?ì„ ?•ì˜?©ë‹ˆ??
//


#include "client/framework.h"
#include "client/Client.h"

#include "client/editor/JScenePanel.h"
#include "client/editor/JFBXLoader.h"

#include "engine/render/JRenderDefinition.h"
#include "engine/core/JEngineContext.h"

J::Render::JWindowInfo s_WindowInfo;

#define MAX_LOADSTRING 100

// ?„ì—­ ë³€??
HINSTANCE hInst;                                // ?„ì¬ ?¸ìŠ¤?´ìŠ¤?…ë‹ˆ??
WCHAR szTitle[MAX_LOADSTRING];                  // ?œëª© ?œì‹œì¤??ìŠ¤?¸ì…?ˆë‹¤.
WCHAR szWindowClass[MAX_LOADSTRING];            // ê¸°ë³¸ ì°??´ë˜???´ë¦„?…ë‹ˆ??

// ??ì½”ë“œ ëª¨ë“ˆ???¬í•¨???¨ìˆ˜??? ì–¸???„ë‹¬?©ë‹ˆ??
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	// ì½˜ì†” ì°??ì„±
	AllocConsole();

	// ?œì? ?…ì¶œ?? ?ëŸ¬ ?¤íŠ¸ë¦¼ì„ ì½˜ì†”???°ê²°
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

    // ?„ì—­ ë¬¸ì?´ì„ ì´ˆê¸°?”í•©?ˆë‹¤.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CLIENT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // ? í”Œë¦¬ì??´ì…˜ ì´ˆê¸°?”ë? ?˜í–‰?©ë‹ˆ??
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CLIENT));

    MSG msg;

    //SetSize
    s_WindowInfo.width = 1920;
    s_WindowInfo.height = 1080;
    s_WindowInfo.windowed = true;
    RECT clientRect{};
    if (GetClientRect(s_WindowInfo.hwnd, &clientRect))
    {
        s_WindowInfo.width = static_cast<int>(clientRect.right - clientRect.left);
        s_WindowInfo.height = static_cast<int>(clientRect.bottom - clientRect.top);
    }

    //SwapChain Create
    J::Render::JSwapChain* swapChain = new J::Render::JSwapChain();
    J::Render::JCommandQueue* cmdQueue = new J::Render::JCommandQueue();

    InitializeEngine(cmdQueue, swapChain);

    cmdQueue->Initialize(GetEngine()->GetDevice()->GetDevice(), swapChain);
    swapChain->Initialize(s_WindowInfo, s_Engine->GetDevice(),cmdQueue->GetCmdQueue());

    unique_ptr<J::Editor::JEditorPanel> panel = make_unique<J::Editor::JScenePanel>();
    panel->Init();

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
		// Wait until frame commands are complete.  This waiting is inefficient and is
	    // done for simplicity.  Later we will show how to organize our rendering code
	    // so we do not have to wait per frame.
		cmdQueue->WaitSync();
        swapChain->SwapIndex();
    }

    panel.reset();
    DestroyEngine();
    return (int) msg.wParam;
}



//
//  ?¨ìˆ˜: MyRegisterClass()
//
//  ?©ë„: ì°??´ë˜?¤ë? ?±ë¡?©ë‹ˆ??
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CLIENT));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = nullptr;
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   ?¨ìˆ˜: InitInstance(HINSTANCE, int)
//
//   ?©ë„: ?¸ìŠ¤?´ìŠ¤ ?¸ë“¤???€?¥í•˜ê³?ì£?ì°½ì„ ë§Œë“­?ˆë‹¤.
//
//   ì£¼ì„:
//
//        ???¨ìˆ˜ë¥??µí•´ ?¸ìŠ¤?´ìŠ¤ ?¸ë“¤???„ì—­ ë³€?˜ì— ?€?¥í•˜ê³?
//        ì£??„ë¡œê·¸ë¨ ì°½ì„ ë§Œë“  ?¤ìŒ ?œì‹œ?©ë‹ˆ??
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // ?¸ìŠ¤?´ìŠ¤ ?¸ë“¤???„ì—­ ë³€?˜ì— ?€?¥í•©?ˆë‹¤.

   RECT windowRect = { 0, 0, 1920, 1080 };
   AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr, nullptr, hInstance, nullptr);

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

//
//  ?¨ìˆ˜: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ?©ë„: ì£?ì°½ì˜ ë©”ì‹œì§€ë¥?ì²˜ë¦¬?©ë‹ˆ??
//
//  WM_COMMAND  - ? í”Œë¦¬ì??´ì…˜ ë©”ë‰´ë¥?ì²˜ë¦¬?©ë‹ˆ??
//  WM_PAINT    - ì£?ì°½ì„ ê·¸ë¦½?ˆë‹¤.
//  WM_DESTROY  - ì¢…ë£Œ ë©”ì‹œì§€ë¥?ê²Œì‹œ?˜ê³  ë°˜í™˜?©ë‹ˆ??
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // ë©”ë‰´ ? íƒ??êµ¬ë¬¸ ë¶„ì„?©ë‹ˆ??
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
            // TODO: ?¬ê¸°??hdcë¥??¬ìš©?˜ëŠ” ê·¸ë¦¬ê¸?ì½”ë“œë¥?ì¶”ê??©ë‹ˆ??..
            EndPaint(hWnd, &ps);
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

// ?•ë³´ ?€???ì??ë©”ì‹œì§€ ì²˜ë¦¬ê¸°ì…?ˆë‹¤.
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






