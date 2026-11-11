#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <shellapi.h>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../shared/HookProtocol.h"

namespace {

struct HookPair {
    HHOOK callWnd = nullptr;
    HHOOK callWndRet = nullptr;
};

HMODULE g_hookDll = nullptr;
HOOKPROC g_callWndProc = nullptr;
HOOKPROC g_callWndRetProc = nullptr;
UINT g_msgAttach = 0;
UINT g_msgDetach = 0;
HANDLE g_parentProcess = nullptr;
DWORD g_parentPid = 0;
std::unordered_map<uint64_t, HookPair> g_hooks;

constexpr UINT_PTR kParentWatchTimerId = 1;

bool LoadHookDll() {
    wchar_t path[MAX_PATH]{};
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return false;

    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) return false;
    *(slash + 1) = L'\0';

    if (wcslen(path) + wcslen(L"FlexWinHook32.dll") >= MAX_PATH) return false;
    wcscat_s(path, L"FlexWinHook32.dll");

    g_hookDll = LoadLibraryW(path);
    if (!g_hookDll) return false;

    g_callWndProc = reinterpret_cast<HOOKPROC>(GetProcAddress(g_hookDll, "FlexWinCallWndProc"));
    g_callWndRetProc = reinterpret_cast<HOOKPROC>(GetProcAddress(g_hookDll, "FlexWinCallWndRetProc"));
    return g_callWndProc != nullptr && g_callWndRetProc != nullptr;
}

void DetachOne(uint64_t hwndU64) {
    auto it = g_hooks.find(hwndU64);
    if (it == g_hooks.end()) return;

    HWND target = reinterpret_cast<HWND>(static_cast<uintptr_t>(hwndU64));
    if (target && IsWindow(target) && g_msgDetach != 0) {
        DWORD_PTR res = 0;
        SendMessageTimeoutW(target, g_msgDetach, 0, 0, SMTO_ABORTIFHUNG | SMTO_NORMAL, 250, &res);
    }

    if (it->second.callWnd) UnhookWindowsHookEx(it->second.callWnd);
    if (it->second.callWndRet) UnhookWindowsHookEx(it->second.callWndRet);
    g_hooks.erase(it);
}

void DetachAll() {
    std::vector<uint64_t> keys;
    keys.reserve(g_hooks.size());
    for (const auto& kv : g_hooks) {
        keys.push_back(kv.first);
    }
    for (uint64_t k : keys) {
        DetachOne(k);
    }
}

LRESULT AttachOne(const flexwin::HelperIpcCommand& cmd) {
    if (!g_hookDll || !g_callWndProc || !g_callWndRetProc) return 0;
    if (cmd.hwnd == 0 || cmd.threadId == 0) return 0;

    HWND target = reinterpret_cast<HWND>(static_cast<uintptr_t>(cmd.hwnd));
    if (!IsWindow(target)) return 0;

    if (g_hooks.find(cmd.hwnd) == g_hooks.end()) {
        HookPair pair{};
        pair.callWnd = SetWindowsHookExW(WH_CALLWNDPROC, g_callWndProc, g_hookDll, cmd.threadId);
        pair.callWndRet = SetWindowsHookExW(WH_CALLWNDPROCRET, g_callWndRetProc, g_hookDll, cmd.threadId);
        if (!pair.callWnd && !pair.callWndRet) {
            return 0;
        }
        g_hooks[cmd.hwnd] = pair;
    }

    if (g_msgAttach != 0) {
        const DWORD pidArg = (cmd.mainPid != 0) ? cmd.mainPid : g_parentPid;
        DWORD_PTR res = 0;
        SendMessageTimeoutW(target, g_msgAttach, static_cast<WPARAM>(pidArg), 0,
                            SMTO_ABORTIFHUNG | SMTO_NORMAL, 250, &res);
    }
    return 1;
}

DWORD ParseParentPid() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    DWORD pid = 0;
    if (argv) {
        for (int i = 1; i + 1 < argc; ++i) {
            if (wcscmp(argv[i], L"--parent-pid") == 0) {
                pid = static_cast<DWORD>(_wtoi(argv[i + 1]));
                break;
            }
        }
        LocalFree(argv);
    }
    return pid;
}

LRESULT CALLBACK HelperWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COPYDATA: {
        const COPYDATASTRUCT* cds = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        if (!cds || cds->cbData != sizeof(flexwin::HelperIpcCommand) || !cds->lpData) {
            return 0;
        }
        const flexwin::HelperIpcCommand* cmd =
            static_cast<const flexwin::HelperIpcCommand*>(cds->lpData);
        switch (cmd->command) {
        case flexwin::kHelperCmdPing:
            return g_hookDll ? 1 : 0;
        case flexwin::kHelperCmdAttach:
            return AttachOne(*cmd);
        case flexwin::kHelperCmdDetach:
            DetachOne(cmd->hwnd);
            return 1;
        case flexwin::kHelperCmdDetachAll:
            DetachAll();
            return 1;
        case flexwin::kHelperCmdShutdown:
            DetachAll();
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 1;
        default:
            return 0;
        }
    }
    case WM_TIMER:
        if (wParam == kParentWatchTimerId) {
            // Reap hooks for dead windows periodically
            std::vector<uint64_t> dead;
            for (const auto& kv : g_hooks) {
                HWND h = reinterpret_cast<HWND>(static_cast<uintptr_t>(kv.first));
                if (!IsWindow(h)) {
                    dead.push_back(kv.first);
                }
            }
            for (uint64_t k : dead) {
                DetachOne(k);
            }
            if (g_parentProcess && WaitForSingleObject(g_parentProcess, 0) == WAIT_OBJECT_0) {
                DetachAll();
                PostQuitMessage(0);
            }
        }
        return 0;
    case WM_DESTROY:
        DetachAll();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    g_msgAttach = RegisterWindowMessageW(flexwin::kHookAttachMsgName);
    g_msgDetach = RegisterWindowMessageW(flexwin::kHookDetachMsgName);

    g_parentPid = ParseParentPid();
    if (g_parentPid != 0) {
        g_parentProcess = OpenProcess(SYNCHRONIZE, FALSE, g_parentPid);
    }

    if (!LoadHookDll()) {
        if (g_parentProcess) CloseHandle(g_parentProcess);
        return 1;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HelperWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = flexwin::kHelper32WindowClass;
    if (!RegisterClassExW(&wc)) {
        if (g_parentProcess) CloseHandle(g_parentProcess);
        FreeLibrary(g_hookDll);
        return 2;
    }

    wchar_t titleBuf[64]{};
    if (g_parentPid != 0) {
        swprintf_s(titleBuf, L"FlexWinHelper32_%lu", static_cast<unsigned long>(g_parentPid));
    } else {
        wcscpy_s(titleBuf, L"FlexWinHelper32");
    }

    HWND hwnd = CreateWindowExW(
        0, flexwin::kHelper32WindowClass, titleBuf,
        WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
    if (!hwnd) {
        if (g_parentProcess) CloseHandle(g_parentProcess);
        FreeLibrary(g_hookDll);
        return 3;
    }

    ChangeWindowMessageFilterEx(hwnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
    SetTimer(hwnd, kParentWatchTimerId, 1000, nullptr);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    KillTimer(hwnd, kParentWatchTimerId);
    DetachAll();
    if (g_parentProcess) CloseHandle(g_parentProcess);
    if (g_hookDll) FreeLibrary(g_hookDll);
    return 0;
}
