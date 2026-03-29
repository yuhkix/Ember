#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <tchar.h>

#include "app.h"
#include "theme.h"
#include "anim.h"

#pragma comment(lib, "dwmapi.lib")

// Forward declaration for ImGui Win32 backend
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Global HWND for title bar button access
static HWND g_hwnd = nullptr;

HWND get_main_hwnd() { return g_hwnd; }

// Custom title bar height (pixels)
static constexpr int TITLE_BAR_HEIGHT = 32;
// Resize border width (pixels)
static constexpr int RESIZE_BORDER    = 6;
// Title bar button width (pixels)
static constexpr int TITLE_BTN_WIDTH  = 46;

// DX11 globals
static ID3D11Device*            g_pd3dDevice           = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext    = nullptr;
static IDXGISwapChain*          g_pSwapChain           = nullptr;
static ID3D11RenderTargetView*  g_pMainRenderTargetView = nullptr;
static bool                     g_SwapChainOccluded    = false;
static UINT                     g_ResizeWidth           = 0;
static UINT                     g_ResizeHeight          = 0;

// Forward declarations
static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int main() {
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"EmberWindowClass";
    RegisterClassExW(&wc);

    // Calculate centered window position
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int windowW = 1100;
    int windowH = 700;
    int posX = (screenW - windowW) / 2;
    int posY = (screenH - windowH) / 2;

    // Create borderless window with resizable frame
    HWND hwnd = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Ember - Dragon's Prophet IDX Tool",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU,
        posX, posY, windowW, windowH,
        nullptr, nullptr, wc.hInstance, nullptr);

    g_hwnd = hwnd;

    // Extend frame into client area for shadow effect
    MARGINS margins = { 0, 0, 0, 1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // Initialize DX11
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Load custom font (try in order of preference)
    {
        ImFont* font = nullptr;
        const char* font_paths[] = {
            "C:\\Windows\\Fonts\\CascadiaCode.ttf",
            "C:\\Windows\\Fonts\\CascadiaMono.ttf",
            "C:\\Windows\\Fonts\\segoeui.ttf",
        };
        for (const char* path : font_paths) {
            if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
                font = io.Fonts->AddFontFromFileTTF(path, 16.0f);
                if (font) break;
            }
        }
        // If no font loaded, ImGui uses its built-in default
    }

    // Apply custom theme
    theme::apply_dark_red();

    // Setup platform/renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Initialize animation system
    anim::init();

    // Create application instance
    App app;

    // Clear color
    const float clear_color[4] = { 0.051f, 0.051f, 0.051f, 1.0f }; // #0D0D0D — match theme bg

    // Main loop
    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                running = false;
        }
        if (!running)
            break;

        // Handle window resize
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = 0;
            g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Handle swap chain being occluded (minimized, etc.)
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
            Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Start new ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Update animations
        anim::update();

        // Apply window fade alpha
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim::window_fade());

        // Render application
        app.render();

        // Pop window fade
        ImGui::PopStyleVar();

        // Render ImGui
        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pMainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_pMainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present with vsync
        HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// -----------------------------------------------------------------------
// DX11 helpers
// -----------------------------------------------------------------------

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                       = hWnd;
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        createDeviceFlags, featureLevelArray, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);

    if (res == DXGI_ERROR_UNSUPPORTED) {
        // Try WARP driver
        res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            createDeviceFlags, featureLevelArray, 2,
            D3D11_SDK_VERSION, &sd, &g_pSwapChain,
            &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    }

    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain)           { g_pSwapChain->Release();          g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext)    { g_pd3dDeviceContext->Release();   g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice)           { g_pd3dDevice->Release();         g_pd3dDevice = nullptr; }
}

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pMainRenderTargetView);
        pBackBuffer->Release();
    }
}

static void CleanupRenderTarget() {
    if (g_pMainRenderTargetView) {
        g_pMainRenderTargetView->Release();
        g_pMainRenderTargetView = nullptr;
    }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_NCCALCSIZE:
        // Remove the non-client area (title bar) entirely.
        // When wParam is TRUE, returning 0 means "use entire window as client area".
        if (wParam == TRUE) {
            // When maximized, adjust for the invisible resize border so the
            // window doesn't extend beyond the monitor work area.
            if (IsZoomed(hWnd)) {
                NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                // Get the monitor info for the monitor the window is on
                HMONITOR mon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi = {};
                mi.cbSize = sizeof(mi);
                GetMonitorInfo(mon, &mi);
                params->rgrc[0] = mi.rcWork;
            }
            return 0;
        }
        break;

    case WM_NCHITTEST: {
        // Custom hit-testing for borderless window: resize edges + caption drag
        POINT cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT rc;
        GetWindowRect(hWnd, &rc);

        int x = cursor.x - rc.left;
        int y = cursor.y - rc.top;
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        // Resize borders (only when not maximized)
        if (!IsZoomed(hWnd)) {
            if (y < RESIZE_BORDER && x < RESIZE_BORDER) return HTTOPLEFT;
            if (y < RESIZE_BORDER && x > w - RESIZE_BORDER) return HTTOPRIGHT;
            if (y > h - RESIZE_BORDER && x < RESIZE_BORDER) return HTBOTTOMLEFT;
            if (y > h - RESIZE_BORDER && x > w - RESIZE_BORDER) return HTBOTTOMRIGHT;
            if (y < RESIZE_BORDER) return HTTOP;
            if (y > h - RESIZE_BORDER) return HTBOTTOM;
            if (x < RESIZE_BORDER) return HTLEFT;
            if (x > w - RESIZE_BORDER) return HTRIGHT;
        }

        // Title bar area: top TITLE_BAR_HEIGHT pixels
        if (y < TITLE_BAR_HEIGHT) {
            // Right side: 3 buttons (minimize, maximize, close), each TITLE_BTN_WIDTH wide
            int buttons_start = w - (TITLE_BTN_WIDTH * 3);
            if (x >= buttons_start)
                return HTCLIENT; // let ImGui handle button clicks
            return HTCAPTION; // draggable title area
        }

        return HTCLIENT;
    }

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth  = static_cast<UINT>(LOWORD(lParam));
        g_ResizeHeight = static_cast<UINT>(HIWORD(lParam));
        return 0;
    case WM_SYSCOMMAND:
        // Disable ALT application menu
        if ((wParam & 0xFFF0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}
