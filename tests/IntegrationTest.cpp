#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <iostream>
#include <string>

#include "../src/core/ConfigManager.h"
#include "../src/core/WindowEngine.h"

using namespace flexwin;

namespace {

HWND WaitForWindowByTitle(const wchar_t* title, DWORD timeoutMs) {
    ULONGLONG start = GetTickCount64();
    while (GetTickCount64() - start < timeoutMs) {
        HWND h = FindWindowW(L"StubbornCorpWndClass", title);
        if (h && IsWindowVisible(h)) return h;
        Sleep(25);
    }
    return nullptr;
}

bool TestTargetBinary(WindowEngine& engine, const std::wstring& exePath,
                      const wchar_t* title, const char* archLabel) {
    std::wstring cmd = L"\"" + exePath + L"\" " + title;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exePath.c_str(), &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        std::cerr << "[FAIL] Could not launch " << archLabel << " StubbornTestApp\n";
        return false;
    }

    HWND target = WaitForWindowByTitle(title, 5000);
    if (!target) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        std::cerr << "[FAIL] Window did not appear for " << archLabel << "\n";
        return false;
    }

    Sleep(100);

    RECT beforeRc{};
    GetWindowRect(target, &beforeRc);
    const LONG beforeW = beforeRc.right - beforeRc.left;
    const LONG beforeH = beforeRc.bottom - beforeRc.top;

    // Verify naive SetWindowPos WITHOUT FlexWin fails (stays locked at 460x320)
    SetWindowPos(target, nullptr, 150, 150, 920, 680, SWP_NOZORDER | SWP_NOACTIVATE);
    RECT naiveRc{};
    GetWindowRect(target, &naiveRc);
    const LONG naiveW = naiveRc.right - naiveRc.left;
    const LONG naiveH = naiveRc.bottom - naiveRc.top;

    // Test Hook DLL alone (aggressiveForce=false so SWP_NOSENDCHANGING is NOT used) + Opacity 60%
    WindowRule rule;
    rule.id = L"test_rule";
    rule.name = L"Test Stubborn Rule";
    rule.enabled = true;
    rule.matchClass = true;
    rule.className = L"StubbornCorpWndClass";
    rule.matchProcess = false;
    rule.geometryMode = GeometryMode::Pixels;
    rule.monitorMode = MonitorTargetMode::Primary;
    rule.x = 180;
    rule.y = 140;
    rule.width = 920;
    rule.height = 680;
    rule.compensateDwmMargin = false;
    rule.makeResizable = true;
    rule.useHookDll = true;
    rule.aggressiveForce = false;
    rule.unlockHitTest = true;
    rule.lockTargetSize = true;
    rule.scaleChildControls = true;
    rule.enableOpacity = true;
    rule.opacityPercent = 60;
    rule.smoothOpacityFade = false;

    bool applied = engine.ApplyRuleToWindow(target, rule, true);
    Sleep(120);

    RECT afterRc{};
    GetWindowRect(target, &afterRc);
    const LONG afterW = afterRc.right - afterRc.left;
    const LONG afterH = afterRc.bottom - afterRc.top;
    const LONG_PTR styleAfter = GetWindowLongPtrW(target, GWL_STYLE);
    const LONG_PTR exStyleAfter = GetWindowLongPtrW(target, GWL_EXSTYLE);
    BYTE alphaAfter = 255;
    DWORD layeredFlags = 0;
    bool gotAlpha = GetLayeredWindowAttributes(target, nullptr, &alphaAfter, &layeredFlags) != FALSE;
    const LRESULT hitRight = SendMessageW(target, WM_NCHITTEST, 0,
                                          MAKELPARAM(afterRc.right - 2, (afterRc.top + afterRc.bottom) / 2));

    bool reverted = engine.RevertWindow(target);
    Sleep(80);
    RECT revertRc{};
    GetWindowRect(target, &revertRc);
    const LONG_PTR exStyleRevert = GetWindowLongPtrW(target, GWL_EXSTYLE);

    // Test AutoMinimize
    WindowRule minRule = rule;
    minRule.id = L"test_min";
    minRule.action = WindowAction::AutoMinimize;
    minRule.actionDelayMs = 0;
    bool minApplied = engine.ApplyRuleToWindow(target, minRule, true);
    Sleep(120);
    bool isMinimized = (IsIconic(target) != FALSE);
    ShowWindow(target, SW_RESTORE);
    Sleep(80);

    // Test AutoClose
    WindowRule closeRule = rule;
    closeRule.id = L"test_close";
    closeRule.action = WindowAction::AutoClose;
    closeRule.actionDelayMs = 0;
    bool closeApplied = engine.ApplyRuleToWindow(target, closeRule, true);
    DWORD waitExit = WaitForSingleObject(pi.hProcess, 2500);
    bool closedGracefully = (waitExit == WAIT_OBJECT_0);
    if (!closedGracefully) {
        PostMessageW(target, WM_CLOSE, 0, 0);
        WaitForSingleObject(pi.hProcess, 1000);
        TerminateProcess(pi.hProcess, 0);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    std::cout << "[" << archLabel << "] Before=" << beforeW << "x" << beforeH
              << " NaiveSWP=" << naiveW << "x" << naiveH
              << " AfterFlexWin=" << afterW << "x" << afterH
              << " Alpha60=" << static_cast<int>(alphaAfter)
              << " HitTestRight=" << hitRight
              << " Reverted=" << (revertRc.right - revertRc.left) << "x" << (revertRc.bottom - revertRc.top)
              << " Minimized=" << isMinimized
              << " AutoClosed=" << closedGracefully
              << "\n";

    bool ok = applied &&
              (naiveW == 460 && naiveH == 320) &&
              (afterW == 920 && afterH == 680) &&
              ((styleAfter & WS_THICKFRAME) != 0) &&
              ((exStyleAfter & WS_EX_LAYERED) != 0) &&
              gotAlpha && (alphaAfter == 153) &&
              (hitRight == HTRIGHT) &&
              reverted &&
              ((exStyleRevert & WS_EX_LAYERED) == 0) &&
              ((revertRc.right - revertRc.left) == 460 && (revertRc.bottom - revertRc.top) == 320) &&
              minApplied && isMinimized &&
              closeApplied && closedGracefully;
    if (!ok) {
        std::cerr << "[FAIL] Integration assertion failed for " << archLabel << "\n";
    }
    return ok;
}

} // namespace

int main() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FlexWinIntegrationHost";
    RegisterClassExW(&wc);

    HWND hostHwnd = CreateWindowExW(0, wc.lpszClassName, L"Host", WS_POPUP,
                                    0, 0, 0, 0, nullptr, nullptr, wc.hInstance, nullptr);

    AppConfig cfg = ConfigManager::CreateDefaultConfig();
    cfg.monitoringEnabled = false;
    cfg.enableHookEngine = true;

    WindowEngine engine;
    engine.Initialize(hostHwnd, cfg);

    const std::wstring dir = ConfigManager::GetExeDirectory();
    bool ok64 = TestTargetBinary(engine, dir + L"\\StubbornTestApp64.exe", L"StubbornX64", "x64");
    bool ok32 = TestTargetBinary(engine, dir + L"\\StubbornTestApp32.exe", L"StubbornX86", "x86");

    engine.Shutdown();
    DestroyWindow(hostHwnd);
    return (ok64 && ok32) ? 0 : 1;
}
