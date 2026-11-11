#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <cstring>

namespace {

constexpr int kLockedW = 460;
constexpr int kLockedH = 320;
HWND g_childList = nullptr;
HWND g_childBtn = nullptr;

LRESULT CALLBACK StubbornWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_childList = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                                      L"Sabit Kurumsal Pencere Simülasyonu (WM_GETMINMAXINFO + WM_WINDOWPOSCHANGING Kilitli)",
                                      WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
                                      12, 12, 420, 210, hwnd, nullptr, nullptr, nullptr);
        g_childBtn = CreateWindowExW(0, L"BUTTON", L"Tamam",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     330, 235, 100, 32, hwnd, nullptr, nullptr, nullptr);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize = POINT{kLockedW, kLockedH};
        mmi->ptMaxTrackSize = POINT{kLockedW, kLockedH};
        mmi->ptMaxSize = POINT{kLockedW, kLockedH};
        return 0;
    }
    case WM_WINDOWPOSCHANGING: {
        auto* wp = reinterpret_cast<WINDOWPOS*>(lParam);
        if (!(wp->flags & SWP_NOSIZE)) {
            wp->cx = kLockedW;
            wp->cy = kLockedH;
        }
        return 0;
    }
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR cmdLine, int) {
    const wchar_t* title = (cmdLine && cmdLine[0]) ? cmdLine : L"StubbornEnterpriseWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = StubbornWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"StubbornCorpWndClass";
    RegisterClassExW(&wc);

    // Explicitly non-resizable WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU (no WS_THICKFRAME)
    HWND hwnd = CreateWindowExW(
        0, L"StubbornCorpWndClass", title,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        120, 120, kLockedW, kLockedH,
        nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) return 1;

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
