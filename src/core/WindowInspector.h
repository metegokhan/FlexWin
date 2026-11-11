#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

namespace flexwin {

struct WindowInfo {
    HWND hwnd = nullptr;
    DWORD pid = 0;
    DWORD tid = 0;
    std::wstring processName;   // e.g. L"saplogon.exe"
    std::wstring processPath;
    std::wstring className;
    std::wstring title;
    RECT windowRect{};
    RECT visualRect{};
    LONG_PTR style = 0;
    LONG_PTR exStyle = 0;
    bool isVisible = false;
    bool isMinimized = false;
    bool isMaximized = false;
    bool isResizableStyle = false;
    bool isOwnedPopup = false;
    bool isToolWindow = false;
    bool isDialogClass = false;
    bool is32Bit = false;
    bool isElevated = false;
    bool accessDenied = false;
};

class WindowInspector {
public:
    static bool Inspect(HWND hwnd, WindowInfo& outInfo);
    static std::vector<WindowInfo> EnumerateTopLevelWindows(bool includeInvisible = false);

    static bool IsSystemProtectedWindow(HWND hwnd, const std::wstring& className, DWORD pid);
    static bool IsWindowCloaked(HWND hwnd);
    static bool IsCurrentProcessElevated();
    static bool RelaunchAsAdmin();
};

} // namespace flexwin
