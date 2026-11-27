#include "ui/EditorApp.h"
#include "resource.h"

#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <tchar.h>
#include <windows.h>

#include <memory>
#include <string>

#include <shellapi.h>

// Forward declared in imgui_impl_win32.h but not exposed by the header.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                                             WPARAM wParam, LPARAM lParam);

namespace {

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapChain = nullptr;
ID3D11RenderTargetView* g_renderTarget = nullptr;
bool g_swapChainOccluded = false;
UINT g_resizeWidth = 0;
UINT g_resizeHeight = 0;
HWND g_hwnd = nullptr;

void createRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer) {
        g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTarget);
        backBuffer->Release();
    }
}

void cleanupRenderTarget() {
    if (g_renderTarget) {
        g_renderTarget->Release();
        g_renderTarget = nullptr;
    }
}

bool createDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL obtained{};
    const HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd,
        &g_swapChain, &g_device, &obtained, &g_context);
    if (FAILED(res)) return false;
    createRenderTarget();
    return true;
}

void cleanupDeviceD3D() {
    cleanupRenderTarget();
    if (g_swapChain) { g_swapChain->Release(); g_swapChain = nullptr; }
    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }
}

LRESULT WINAPI wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_resizeWidth = static_cast<UINT>(LOWORD(lParam));
        g_resizeHeight = static_cast<UINT>(HIWORD(lParam));
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED: {
        const RECT* r = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hWnd, nullptr, r->left, r->top, r->right - r->left,
                     r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

} // namespace

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"DKEditorWindow";

    const HICON appIcon = static_cast<HICON>(LoadImageW(
        hInstance, MAKEINTRESOURCEW(IDI_DKEDITOR), IMAGE_ICON, 0, 0,
        LR_DEFAULTSIZE | LR_SHARED));
    const HICON appIconSmall = static_cast<HICON>(LoadImageW(
        hInstance, MAKEINTRESOURCEW(IDI_DKEDITOR), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
        LR_SHARED));
    wc.hIcon = appIcon;
    wc.hIconSm = appIconSmall;
    RegisterClassExW(&wc);

    const int width = 1600;
    const int height = 940;
    RECT rect{0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    g_hwnd = CreateWindowW(wc.lpszClassName, L"DK Editor - Donkey Kong Country 1/2/3",
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           rect.right - rect.left, rect.bottom - rect.top, nullptr,
                           nullptr, hInstance, nullptr);
    if (!g_hwnd) return 1;
    if (appIcon) SendMessageW(g_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(appIcon));
    if (appIconSmall) SendMessageW(g_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(appIconSmall));

    if (!createDeviceD3D(g_hwnd)) {
        cleanupDeviceD3D();
        MessageBoxW(g_hwnd, L"Could not create the Direct3D 11 device.", L"DK Editor",
                    MB_ICONERROR);
        DestroyWindow(g_hwnd);
        UnregisterClassW(wc.lpszClassName, hInstance);
        return 1;
    }

    ShowWindow(g_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.FramePadding = ImVec2(7, 4);
    style.ItemSpacing = ImVec2(7, 6);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.08f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.085f, 0.09f, 0.115f, 1.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.17f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.29f, 0.42f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.27f, 0.39f, 0.56f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.45f, 0.64f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.21f, 0.28f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.33f, 0.45f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.31f, 0.42f, 0.58f, 1.00f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.12f, 0.16f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.14f, 1.00f);

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    auto app = std::make_unique<dk::EditorApp>(g_device, g_context);
    app->setWindow(g_hwnd);

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1) {
        const int n = WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, nullptr, 0, nullptr, nullptr);
        if (n > 1) {
            std::string path(static_cast<std::size_t>(n - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, argv[1], -1, path.data(), n, nullptr, nullptr);
            app->openInitialRom(path);
            if (argc > 2) {
                wchar_t* end = nullptr;
                const unsigned long id = wcstoul(argv[2], &end, 0);
                if (end && *end == L'\0') app->selectLevel(static_cast<int>(id));
            }
        }
    }
    if (argv) LocalFree(argv);

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (g_swapChainOccluded && g_swapChain->Present(0, DXGI_PRESENT_TEST) ==
                                       DXGI_STATUS_OCCLUDED) {
            Sleep(10);
            continue;
        }
        g_swapChainOccluded = false;

        if (g_resizeWidth != 0 && g_resizeHeight != 0) {
            cleanupRenderTarget();
            g_swapChain->ResizeBuffers(0, g_resizeWidth, g_resizeHeight,
                                       DXGI_FORMAT_UNKNOWN, 0);
            g_resizeWidth = g_resizeHeight = 0;
            createRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        app->draw();

        ImGui::Render();
        const ImVec4 clear(0.055f, 0.06f, 0.075f, 1.0f);
        g_context->OMSetRenderTargets(1, &g_renderTarget, nullptr);
        g_context->ClearRenderTargetView(g_renderTarget, &clear.x);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        const HRESULT hr = g_swapChain->Present(1, 0);
        g_swapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    app.reset();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanupDeviceD3D();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, hInstance);
    return 0;
}
