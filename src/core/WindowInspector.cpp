#include "WindowInspector.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <vector>

#include "MonitorManager.h"
#include "StringUtil.h"
#include "../../shared/HookProtocol.h"

namespace flexwin {

namespace {

using IsWow64Process2Fn = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);

bool QueryProcessIs32Bit(HANDLE hProcess) {
    static IsWow64Process2Fn fn2 = []() -> IsWow64Process2Fn {
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        return k32 ? reinterpret_cast<IsWow64Process2Fn>(GetProcAddress(k32, "IsWow64Process2")) : nullptr;
    }();

    if (fn2) {
        USHORT processMachine = 0;
        USHORT nativeMachine = 0;
        if (fn2(hProcess, &processMachine, &nativeMachine)) {
            if (processMachine != IMAGE_FILE_MACHINE_UNKNOWN) {
                return (processMachine == IMAGE_FILE_MACHINE_I386);
            }
            return (nativeMachine == IMAGE_FILE_MACHINE_I386);
        }
    }

    BOOL wow64 = FALSE;
    if (IsWow64Process(hProcess, &wow64)) {
        return wow64 != FALSE;
    }
    return false;
}

bool QueryProcessElevated(HANDLE hProcess, bool& outAccessDenied) {
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            outAccessDenied = true;
        }
        return false;
    }
    TOKEN_ELEVATION elev{};
    DWORD retLen = 0;
    bool elevated = false;
    if (GetTokenInformation(hToken, TokenElevation, &elev, sizeof(elev), &retLen)) {
        elevated = (elev.TokenIsElevated != 0);
    }
    CloseHandle(hToken);
    return elevated;
}

std::wstring GetWindowClassSafe(HWND hwnd) {
    wchar_t buf[256]{};
    int len = GetClassNameW(hwnd, buf, 256);
    if (len <= 0) return {};
    return std::wstring(buf, static_cast<size_t>(len));
}

std::wstring GetWindowTitleSafe(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return {};
    std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
    int copied = GetWindowTextW(hwnd, &buf[0], len + 1);
    if (copied <= 0) return {};
    buf.resize(static_cast<size_t>(copied));
    return buf;
}

} // namespace

bool WindowInspector::IsWindowCloaked(HWND hwnd) {
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
        return cloaked != 0;
    }
    return false;
}

bool WindowInspector::IsSystemProtectedWindow(HWND hwnd, const std::wstring& className, DWORD pid) {
    if (!hwnd || !IsWindow(hwnd)) return true;
    if (pid == GetCurrentProcessId()) return true;

    static const wchar_t* const kBlockedClasses[] = {
        L"Shell_TrayWnd",
        L"Shell_SecondaryTrayWnd",
        L"Progman",
        L"WorkerW",
        L"DV2ControlHost",
        L"Windows.UI.Core.CoreWindow",
        L"ApplicationFrameInputSinkWindow",
        L"NotifyIconOverflowWindow",
        L"TopLevelWindowForOverflowXamlIsland",
        L"Xaml_WindowedPopupClass",
        L"TaskListThumbnailWnd",
        L"MultitaskingViewFrame",
        L"ForegroundStaging",
        L"EdgeUiInputTopWndClass",
        L"EdgeUiInputWndClass",
        L"UAC_InputIndicatorOverlayWnd",
        L"UAC Input Indicator",
        L"Touch Tooltip Window",
        L"IME",
        L"MSCTFIME UI",
        L"CiceroUIWndFrame",
        L"CicLoaderWndClass",
        kHelper32WindowClass
    };

    for (const wchar_t* blocked : kBlockedClasses) {
        if (EqualsIgnoreCase(className, blocked)) {
            return true;
        }
    }
    return false;
}

bool WindowInspector::Inspect(HWND hwnd, WindowInfo& outInfo) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    outInfo = WindowInfo{};
    outInfo.hwnd = hwnd;
    outInfo.tid = GetWindowThreadProcessId(hwnd, &outInfo.pid);
    outInfo.className = GetWindowClassSafe(hwnd);
    outInfo.title = GetWindowTitleSafe(hwnd);
    outInfo.style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    outInfo.exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    outInfo.isVisible = (IsWindowVisible(hwnd) != FALSE);
    outInfo.isMinimized = (IsIconic(hwnd) != FALSE);
    outInfo.isMaximized = (IsZoomed(hwnd) != FALSE);
    outInfo.isResizableStyle = ((outInfo.style & WS_THICKFRAME) != 0);
    outInfo.isOwnedPopup = (GetWindow(hwnd, GW_OWNER) != nullptr) || ((outInfo.style & WS_CHILD) != 0);
    outInfo.isToolWindow = ((outInfo.exStyle & WS_EX_TOOLWINDOW) != 0);
    outInfo.isDialogClass = EqualsIgnoreCase(outInfo.className, L"#32770");

    GetWindowRect(hwnd, &outInfo.windowRect);
    outInfo.visualRect = MonitorManager::GetVisualWindowRect(hwnd);

    if (outInfo.pid != 0) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, outInfo.pid);
        if (!hProc) {
            if (GetLastError() == ERROR_ACCESS_DENIED) {
                outInfo.accessDenied = true;
                outInfo.isElevated = true;
            }
        } else {
            wchar_t pathBuf[1024]{};
            DWORD pathLen = 1024;
            if (QueryFullProcessImageNameW(hProc, 0, pathBuf, &pathLen)) {
                outInfo.processPath.assign(pathBuf, pathLen);
                outInfo.processName = BaseFileName(outInfo.processPath);
            }
            outInfo.is32Bit = QueryProcessIs32Bit(hProc);
            bool tokenDenied = false;
            outInfo.isElevated = QueryProcessElevated(hProc, tokenDenied);
            if (tokenDenied) {
                outInfo.accessDenied = true;
                outInfo.isElevated = true;
            }
            CloseHandle(hProc);
        }
    }

    return true;
}

std::vector<WindowInfo> WindowInspector::EnumerateTopLevelWindows(bool includeInvisible) {
    struct EnumCtx {
        std::vector<WindowInfo>* out;
        bool includeInvisible;
    } ctx{nullptr, includeInvisible};

    std::vector<WindowInfo> result;
    ctx.out = &result;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto* c = reinterpret_cast<EnumCtx*>(lParam);
        if (!c->includeInvisible) {
            if (!IsWindowVisible(hwnd)) return TRUE;
            if (IsWindowCloaked(hwnd)) return TRUE;
        }
        WindowInfo info;
        if (!WindowInspector::Inspect(hwnd, info)) return TRUE;
        if (WindowInspector::IsSystemProtectedWindow(hwnd, info.className, info.pid)) return TRUE;

        const LONG w = info.windowRect.right - info.windowRect.left;
        const LONG h = info.windowRect.bottom - info.windowRect.top;
        if (!c->includeInvisible && (w <= 1 || h <= 1)) return TRUE;
        if (!c->includeInvisible && info.title.empty() && info.isToolWindow) return TRUE;

        c->out->push_back(std::move(info));
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    return result;
}

bool WindowInspector::IsCurrentProcessElevated() {
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        return false;
    }
    TOKEN_ELEVATION elev{};
    DWORD retLen = 0;
    bool elevated = false;
    if (GetTokenInformation(hToken, TokenElevation, &elev, sizeof(elev), &retLen)) {
        elevated = (elev.TokenIsElevated != 0);
    }
    CloseHandle(hToken);
    return elevated;
}

bool WindowInspector::RelaunchAsAdmin() {
    wchar_t exePath[MAX_PATH]{};
    DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return false;

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.lpParameters = L"--elevated-restart";
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

} // namespace flexwin
