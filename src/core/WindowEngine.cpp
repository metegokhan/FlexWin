#include "WindowEngine.h"

#include <algorithm>
#include <cmath>
#include <regex>

#include "I18n.h"
#include "Logger.h"
#include "StringUtil.h"

namespace flexwin {

constexpr UINT WM_APP_START_ANIM_TIMER = WM_APP + 104;

namespace {

WindowEngine* g_engineInstance = nullptr;

void CALLBACK WinEventCallback(HWINEVENTHOOK, DWORD event, HWND hwnd,
                               LONG idObject, LONG idChild, DWORD, DWORD) {
    if (g_engineInstance) {
        g_engineInstance->OnWinEvent(event, hwnd, idObject, idChild);
    }
}

bool MatchProcessPattern(const std::wstring& patternList, const WindowInfo& info) {
    if (patternList.empty()) return false;
    size_t start = 0;
    while (start <= patternList.size()) {
        size_t sep = patternList.find(L';', start);
        std::wstring token = (sep == std::wstring::npos)
                                 ? patternList.substr(start)
                                 : patternList.substr(start, sep - start);
        token = TrimWide(token);
        if (!token.empty()) {
            const bool hasSlash = (token.find(L'\\') != std::wstring::npos) ||
                                  (token.find(L'/') != std::wstring::npos);
            if (hasSlash) {
                if (EqualsIgnoreCase(token, info.processPath)) return true;
            } else {
                if (EqualsIgnoreCase(token, info.processName)) return true;
            }
        }
        if (sep == std::wstring::npos) break;
        start = sep + 1;
    }
    return false;
}

bool MatchTitle(TitleMatchMode mode, const std::wstring& pattern, const std::wstring& title) {
    switch (mode) {
    case TitleMatchMode::Ignore:
        return true;
    case TitleMatchMode::Contains:
        return ContainsIgnoreCase(title, pattern);
    case TitleMatchMode::Exact:
        return EqualsIgnoreCase(title, pattern);
    case TitleMatchMode::StartsWith:
        return StartsWithIgnoreCase(title, pattern);
    case TitleMatchMode::Regex:
        if (pattern.empty()) return true;
        try {
            std::wregex re(pattern, std::regex_constants::icase);
            return std::regex_search(title, re);
        } catch (const std::regex_error&) {
            return false;
        }
    }
    return false;
}

BYTE PercentToAlpha(int pct) {
    const int clamped = std::clamp(pct, 10, 100);
    return static_cast<BYTE>(std::lround(clamped * 255.0 / 100.0));
}

} // namespace

WindowEngine::~WindowEngine() {
    Shutdown();
}

bool WindowEngine::Initialize(HWND controllerHwnd, AppConfig config) {
    m_controllerHwnd = controllerHwnd;
    m_config = std::move(config);
    g_engineInstance = this;

    m_hookBridge.Initialize(controllerHwnd);
    if (m_config.monitoringEnabled) {
        StartEventHooks();
    }
    return true;
}

void WindowEngine::Shutdown() {
    StopEventHooks();
    m_hookBridge.Shutdown();
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_tracked.clear();
        m_pendingActions.clear();
        m_activeFades.clear();
    }
    if (g_engineInstance == this) {
        g_engineInstance = nullptr;
    }
}

void WindowEngine::UpdateConfig(const AppConfig& config) {
    const bool wasMonitoring = m_config.monitoringEnabled;
    const bool hooksWereEnabled = m_config.enableHookEngine;
    m_config = config;

    if (!m_config.enableHookEngine && hooksWereEnabled) {
        m_hookBridge.DetachAll();
    }

    if (m_config.monitoringEnabled && !wasMonitoring) {
        StartEventHooks();
    } else if (!m_config.monitoringEnabled && wasMonitoring) {
        StopEventHooks();
    }
}

void WindowEngine::StartEventHooks() {
    if (m_eventHookShow) return;

    m_eventHookShow = SetWinEventHook(
        EVENT_OBJECT_SHOW, EVENT_OBJECT_NAMECHANGE, nullptr, WinEventCallback,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    m_eventHookLoc = SetWinEventHook(
        EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, WinEventCallback,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    m_eventHookDestroy = SetWinEventHook(
        EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY, nullptr, WinEventCallback,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    if (!m_eventHookShow) {
        Logger::Instance().Error(
            I18n::GetLanguage() == Language::En
                ? L"Failed to start WinEventHook."
                : L"WinEventHook başlatılamadı.");
    }
}

void WindowEngine::StopEventHooks() {
    if (m_eventHookShow) {
        UnhookWinEvent(m_eventHookShow);
        m_eventHookShow = nullptr;
    }
    if (m_eventHookLoc) {
        UnhookWinEvent(m_eventHookLoc);
        m_eventHookLoc = nullptr;
    }
    if (m_eventHookDestroy) {
        UnhookWinEvent(m_eventHookDestroy);
        m_eventHookDestroy = nullptr;
    }
}

const Profile* WindowEngine::GetActiveProfile() const {
    if (m_config.profiles.empty()) return nullptr;
    int idx = std::clamp(m_config.activeProfileIndex, 0,
                         static_cast<int>(m_config.profiles.size()) - 1);
    return &m_config.profiles[static_cast<size_t>(idx)];
}

bool WindowEngine::MatchesRule(const WindowRule& rule, const WindowInfo& info, bool ignoreEnabled) {
    if (!ignoreEnabled && !rule.enabled) return false;

    const bool hasAnyCondition =
        (rule.matchProcess && !TrimWide(rule.processName).empty()) ||
        (rule.matchClass && !TrimWide(rule.className).empty()) ||
        (rule.titleMode != TitleMatchMode::Ignore && !rule.titlePattern.empty());
    if (!hasAnyCondition) return false;

    if (rule.matchProcess && !MatchProcessPattern(rule.processName, info)) return false;
    if (rule.matchClass && !EqualsIgnoreCase(TrimWide(rule.className), info.className)) return false;
    if (rule.titleMode != TitleMatchMode::Ignore &&
        !MatchTitle(rule.titleMode, rule.titlePattern, info.title)) {
        return false;
    }

    if (rule.excludePopupsAndDialogs) {
        if (info.isToolWindow || info.isOwnedPopup) return false;
        const bool explicitlyTargetsDialog =
            rule.matchClass && EqualsIgnoreCase(TrimWide(rule.className), L"#32770");
        if (info.isDialogClass && !explicitlyTargetsDialog) return false;
    }

    if (!info.isMinimized) {
        const LONG curW = info.windowRect.right - info.windowRect.left;
        const LONG curH = info.windowRect.bottom - info.windowRect.top;
        if (curW < rule.minSourceWidth || curH < rule.minSourceHeight) return false;
    }

    return true;
}

const WindowRule* WindowEngine::FindMatchingRule(const WindowInfo& info) const {
    const Profile* prof = GetActiveProfile();
    if (!prof) return nullptr;
    for (const auto& rule : prof->rules) {
        if (MatchesRule(rule, info, false)) {
            return &rule;
        }
    }
    return nullptr;
}

int WindowEngine::ApplyActiveProfileToAllWindows() {
    const Profile* prof = GetActiveProfile();
    if (!prof) return 0;

    auto windows = WindowInspector::EnumerateTopLevelWindows(false);
    int applied = 0;
    for (const auto& info : windows) {
        for (const auto& rule : prof->rules) {
            if (MatchesRule(rule, info, false)) {
                if (ApplyRuleToWindow(info.hwnd, rule, false)) {
                    ++applied;
                }
                break;
            }
        }
    }
    return applied;
}

void WindowEngine::CaptureSnapshotIfNeeded(HWND hwnd, const WindowInfo& info,
                                           TrackedWindow& tracked, bool captureChildren) {
    if (!tracked.snapshot.captured) {
        tracked.snapshot.origWindowRect = info.windowRect;
        tracked.snapshot.origStyle = info.style;
        tracked.snapshot.origExStyle = info.exStyle;
        tracked.snapshot.origHadLayered = ((info.exStyle & WS_EX_LAYERED) != 0);
        if (tracked.snapshot.origHadLayered) {
            BYTE alpha = 255;
            DWORD flags = 0;
            if (GetLayeredWindowAttributes(hwnd, nullptr, &alpha, &flags)) {
                tracked.snapshot.origAlpha = alpha;
                tracked.snapshot.origLayeredFlags = flags;
            }
        }
        tracked.snapshot.captured = true;
    }
    if (captureChildren && tracked.snapshot.children.empty()) {
        RECT clientRc{};
        if (GetClientRect(hwnd, &clientRc)) {
            tracked.snapshot.origClientW = clientRc.right - clientRc.left;
            tracked.snapshot.origClientH = clientRc.bottom - clientRc.top;
            HWND child = GetWindow(hwnd, GW_CHILD);
            while (child) {
                RECT cr{};
                if (GetWindowRect(child, &cr)) {
                    MapWindowPoints(HWND_DESKTOP, hwnd, reinterpret_cast<LPPOINT>(&cr), 2);
                    tracked.snapshot.children.push_back(ChildSnapshot{child, cr});
                }
                child = GetWindow(child, GW_HWNDNEXT);
            }
        }
    }
}

void WindowEngine::ScaleDirectChildren(HWND hwnd, TrackedWindow& tracked) {
    if (tracked.snapshot.children.empty() ||
        tracked.snapshot.origClientW <= 0 || tracked.snapshot.origClientH <= 0) {
        return;
    }

    RECT curClient{};
    if (!GetClientRect(hwnd, &curClient)) return;
    const int curW = curClient.right - curClient.left;
    const int curH = curClient.bottom - curClient.top;
    if (curW <= 0 || curH <= 0) return;

    const double sx = static_cast<double>(curW) / static_cast<double>(tracked.snapshot.origClientW);
    const double sy = static_cast<double>(curH) / static_cast<double>(tracked.snapshot.origClientH);

    HDWP hdwp = BeginDeferWindowPos(static_cast<int>(tracked.snapshot.children.size()));
    if (!hdwp) return;

    for (const auto& ch : tracked.snapshot.children) {
        if (!IsWindow(ch.child)) continue;
        const int nx = static_cast<int>(std::lround(ch.origRect.left * sx));
        const int ny = static_cast<int>(std::lround(ch.origRect.top * sy));
        const int nw = (std::max)(8, static_cast<int>(std::lround((ch.origRect.right - ch.origRect.left) * sx)));
        const int nh = (std::max)(8, static_cast<int>(std::lround((ch.origRect.bottom - ch.origRect.top) * sy)));
        hdwp = DeferWindowPos(hdwp, ch.child, nullptr, nx, ny, nw, nh,
                              SWP_NOZORDER | SWP_NOACTIVATE);
        if (!hdwp) break;
    }
    if (hdwp) {
        EndDeferWindowPos(hdwp);
    }
}

bool WindowEngine::SetWindowOpacity(HWND hwnd, int percent, bool smooth) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    const BYTE targetAlpha = PercentToAlpha(percent);
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    BYTE startAlpha = 255;
    if (ex & WS_EX_LAYERED) {
        DWORD fl = 0;
        if (!GetLayeredWindowAttributes(hwnd, nullptr, &startAlpha, &fl) || !(fl & LWA_ALPHA)) {
            startAlpha = 255;
        }
    } else {
        SetLastError(0);
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            return false;
        }
    }

    if (!smooth || !m_controllerHwnd || !IsWindow(m_controllerHwnd) || std::abs(targetAlpha - startAlpha) < 8) {
        return SetLayeredWindowAttributes(hwnd, 0, targetAlpha, LWA_ALPHA) != FALSE;
    }

    SetLayeredWindowAttributes(hwnd, 0, startAlpha, LWA_ALPHA);
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        bool updated = false;
        for (auto& f : m_activeFades) {
            if (f.hwnd == hwnd) {
                f.currentAlpha = startAlpha;
                f.targetAlpha = targetAlpha;
                f.stepPerTick = (std::max)(8, std::abs(targetAlpha - startAlpha) / 7);
                updated = true;
                break;
            }
        }
        if (!updated) {
            m_activeFades.push_back(OpacityFadeTask{
                hwnd, startAlpha, targetAlpha,
                (std::max)(8, std::abs(targetAlpha - startAlpha) / 7)
            });
        }
    }
    PostMessageW(m_controllerHwnd, WM_APP_START_ANIM_TIMER, 0, 0);
    return true;
}

void WindowEngine::ApplyStyleAndPosition(HWND hwnd, const WindowInfo& info, const WindowRule& rule,
                                         TrackedWindow& tracked, const std::vector<MonitorInfoEx>& monitors) {
    if (info.isMinimized) return;

    const ComputedTargetRect target = MonitorManager::ComputeTargetRect(rule, hwnd, monitors);
    const int targetW = target.adjustedRect.right - target.adjustedRect.left;
    const int targetH = target.adjustedRect.bottom - target.adjustedRect.top;

    if (m_config.enableHookEngine && rule.useHookDll) {
        HookParams hp{};
        hp.flags = kHookFlagUnlockMinMax | kHookFlagAllowResize;
        if (rule.makeResizable && !rule.borderless) hp.flags |= kHookFlagKeepThickFrame;
        if (rule.unlockHitTest) hp.flags |= kHookFlagUnlockHitTest;
        if (rule.lockTargetSize && target.resize && targetW > 0 && targetH > 0) {
            hp.flags |= kHookFlagForceExactSize;
            hp.targetWidth = targetW;
            hp.targetHeight = targetH;
        }
        if (rule.customMinMax) {
            hp.minWidth = rule.customMinW;
            hp.minHeight = rule.customMinH;
            hp.maxWidth = rule.customMaxW;
            hp.maxHeight = rule.customMaxH;
        }
        if (!m_hookBridge.AttachOrUpdate(hwnd, info.tid, info.is32Bit, hp)) {
            if (info.isElevated && !WindowInspector::IsCurrentProcessElevated()) {
                Logger::Instance().Warn(
                    (I18n::GetLanguage() == Language::En
                         ? L"Hook denied (Administrator required): "
                         : L"Hook eklenemedi (Yönetici yetkisi gerekli): ") +
                    info.processName);
            }
        }
    }

    LONG_PTR newStyle = GetWindowLongPtrW(hwnd, GWL_STYLE);
    LONG_PTR wantedStyle = newStyle;
    if (rule.borderless) {
        wantedStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER | WS_DLGFRAME);
    } else if (rule.makeResizable) {
        wantedStyle |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
    }

    if (wantedStyle != newStyle) {
        SetLastError(0);
        SetWindowLongPtrW(hwnd, GWL_STYLE, wantedStyle);
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            Logger::Instance().Warn(
                (I18n::GetLanguage() == Language::En
                     ? L"Style denied (Administrator required): "
                     : L"Stil değiştirilemedi (UIPI / Yönetici gerekli): ") +
                info.processName);
            return;
        }
    }

    if (info.isMaximized && (target.move || target.resize)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    UINT swpFlags = SWP_NOACTIVATE | SWP_FRAMECHANGED;
    if (!rule.alwaysOnTop) swpFlags |= SWP_NOZORDER;
    if (rule.aggressiveForce) swpFlags |= SWP_NOSENDCHANGING;
    if (!target.move) swpFlags |= SWP_NOMOVE;
    if (!target.resize) swpFlags |= SWP_NOSIZE;

    tracked.suppressUntilTick = GetTickCount64() + 200;
    if (target.move || target.resize) {
        tracked.lastTargetAdjustedRect = target.adjustedRect;
        tracked.hasTargetRect = true;
    }

    const HWND insertAfter = rule.alwaysOnTop ? HWND_TOPMOST : nullptr;
    const BOOL ok = SetWindowPos(hwnd, insertAfter,
                                 target.adjustedRect.left, target.adjustedRect.top,
                                 (std::max)(1, targetW), (std::max)(1, targetH),
                                 swpFlags);
    if (!ok && GetLastError() == ERROR_ACCESS_DENIED) {
        Logger::Instance().Warn(
            (I18n::GetLanguage() == Language::En
                 ? L"Move/Resize denied (Administrator required): "
                 : L"Konum/boyut uygulanamadı (Yönetici izni gerekli): ") +
            info.processName);
        return;
    }

    if (rule.scaleChildControls && target.resize) {
        ScaleDirectChildren(hwnd, tracked);
    }

    if (rule.enableOpacity) {
        SetWindowOpacity(hwnd, rule.opacityPercent, rule.smoothOpacityFade);
    }
}

void WindowEngine::ExecuteLifecycleAction(HWND hwnd, const WindowInfo& info,
                                          const WindowRule& rule, bool isManualTest) {
    if (rule.action == WindowAction::AutoMinimize) {
        ShowWindowAsync(hwnd, SW_MINIMIZE);
        Logger::Instance().Success(
            std::wstring(isManualTest ? L"[Test/Min] " : L"[Oto/Min] ") +
            rule.name + L" -> " + info.processName);
    } else if (rule.action == WindowAction::AutoClose) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        Logger::Instance().Success(
            std::wstring(isManualTest ? L"[Test/Kapat] " : L"[Oto/Kapat] ") +
            rule.name + L" -> " + info.processName);
    }
}

bool WindowEngine::ApplyRuleToWindow(HWND hwnd, const WindowRule& rule, bool isManualTest) {
    WindowInfo info;
    if (!WindowInspector::Inspect(hwnd, info)) return false;
    if (WindowInspector::IsSystemProtectedWindow(hwnd, info.className, info.pid)) return false;
    if (!info.isVisible) return false;

    const uint64_t key = HwndToU64(hwnd);
    TrackedWindow trackedCopy;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        TrackedWindow& tr = m_tracked[key];
        CaptureSnapshotIfNeeded(hwnd, info, tr, rule.scaleChildControls);
        tr.appliedRuleId = rule.id;
        tr.firstAppliedTick = GetTickCount64();
        tr.lastReapplyWindowStart = tr.firstAppliedTick;
        tr.reapplyCountInBurst = 0;
        trackedCopy = tr;
    }

    if (rule.action == WindowAction::AutoMinimize || rule.action == WindowAction::AutoClose) {
        if (!isManualTest && rule.actionDelayMs > 0 && m_controllerHwnd && IsWindow(m_controllerHwnd)) {
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                bool exists = false;
                for (auto& pa : m_pendingActions) {
                    if (pa.hwnd == hwnd) {
                        pa.rule = rule;
                        pa.executeAtTick = GetTickCount64() + static_cast<ULONGLONG>(rule.actionDelayMs);
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    m_pendingActions.push_back(PendingDelayedAction{
                        hwnd, rule, GetTickCount64() + static_cast<ULONGLONG>(rule.actionDelayMs)
                    });
                }
            }
            PostMessageW(m_controllerHwnd, WM_APP_START_ANIM_TIMER, 0, 0);
            return true;
        }
        ExecuteLifecycleAction(hwnd, info, rule, isManualTest);
        return true;
    }

    const auto monitors = MonitorManager::EnumerateMonitors();
    ApplyStyleAndPosition(hwnd, info, rule, trackedCopy, monitors);

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto it = m_tracked.find(key);
        if (it != m_tracked.end()) {
            it->second.suppressUntilTick = trackedCopy.suppressUntilTick;
            it->second.lastTargetAdjustedRect = trackedCopy.lastTargetAdjustedRect;
            it->second.hasTargetRect = trackedCopy.hasTargetRect;
        }
    }

    Logger::Instance().Success(
        std::wstring(isManualTest ? L"[Test] " : L"[Oto] ") +
        rule.name + L" -> " + info.processName +
        (info.is32Bit ? L" (x86)" : L" (x64)"));
    return true;
}

bool WindowEngine::RevertWindow(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return false;
    const uint64_t key = HwndToU64(hwnd);

    WindowStateSnapshot snap{};
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto it = m_tracked.find(key);
        if (it == m_tracked.end() || !it->second.snapshot.captured) {
            m_hookBridge.Detach(hwnd);
            return false;
        }
        snap = it->second.snapshot;
        m_tracked.erase(it);

        m_activeFades.erase(
            std::remove_if(m_activeFades.begin(), m_activeFades.end(),
                           [hwnd](const OpacityFadeTask& f) { return f.hwnd == hwnd; }),
            m_activeFades.end());
    }

    m_hookBridge.Detach(hwnd);

    SetWindowLongPtrW(hwnd, GWL_STYLE, snap.origStyle);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, snap.origExStyle);
    if (snap.origHadLayered) {
        SetLayeredWindowAttributes(hwnd, 0, snap.origAlpha, snap.origLayeredFlags);
    }

    const HWND zOrder = (snap.origExStyle & WS_EX_TOPMOST) ? HWND_TOPMOST : HWND_NOTOPMOST;
    const int w = snap.origWindowRect.right - snap.origWindowRect.left;
    const int h = snap.origWindowRect.bottom - snap.origWindowRect.top;
    SetWindowPos(hwnd, zOrder, snap.origWindowRect.left, snap.origWindowRect.top,
                 w, h, SWP_NOACTIVATE | SWP_FRAMECHANGED);

    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    if (!snap.children.empty()) {
        HDWP hdwp = BeginDeferWindowPos(static_cast<int>(snap.children.size()));
        if (hdwp) {
            for (const auto& ch : snap.children) {
                if (!IsWindow(ch.child)) continue;
                hdwp = DeferWindowPos(hdwp, ch.child, nullptr,
                                      ch.origRect.left, ch.origRect.top,
                                      ch.origRect.right - ch.origRect.left,
                                      ch.origRect.bottom - ch.origRect.top,
                                      SWP_NOZORDER | SWP_NOACTIVATE);
                if (!hdwp) break;
            }
            if (hdwp) EndDeferWindowPos(hdwp);
        }
    }

    Logger::Instance().Info(
        I18n::GetLanguage() == Language::En
            ? L"Window reverted to previous size, style and opacity."
            : L"Pencere önceki boyut, stil ve şeffaflığına geri alındı.");
    return true;
}

bool WindowEngine::QuickUnlockWindow(HWND hwnd) {
    WindowInfo info;
    if (!WindowInspector::Inspect(hwnd, info)) return false;
    if (WindowInspector::IsSystemProtectedWindow(hwnd, info.className, info.pid)) return false;

    const uint64_t key = HwndToU64(hwnd);
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        CaptureSnapshotIfNeeded(hwnd, info, m_tracked[key], false);
    }

    if (m_config.enableHookEngine) {
        HookParams hp{};
        hp.flags = kHookFlagUnlockMinMax | kHookFlagAllowResize |
                   kHookFlagKeepThickFrame | kHookFlagUnlockHitTest;
        m_hookBridge.AttachOrUpdate(hwnd, info.tid, info.is32Bit, hp);
    }

    LONG_PTR st = GetWindowLongPtrW(hwnd, GWL_STYLE);
    SetWindowLongPtrW(hwnd, GWL_STYLE, st | WS_THICKFRAME | WS_MAXIMIZEBOX);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    Logger::Instance().Success(L"[Unlock] " + info.processName +
                               (info.is32Bit ? L" (x86)" : L" (x64)"));
    return true;
}

bool WindowEngine::QuickToggleBorderless(HWND hwnd) {
    WindowInfo info;
    if (!WindowInspector::Inspect(hwnd, info)) return false;
    if (WindowInspector::IsSystemProtectedWindow(hwnd, info.className, info.pid)) return false;

    const uint64_t key = HwndToU64(hwnd);
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        CaptureSnapshotIfNeeded(hwnd, info, m_tracked[key], false);
    }

    LONG_PTR st = GetWindowLongPtrW(hwnd, GWL_STYLE);
    if (st & WS_CAPTION) {
        st &= ~(WS_CAPTION | WS_THICKFRAME | WS_BORDER | WS_DLGFRAME);
        Logger::Instance().Info(L"[Borderless On] " + info.processName);
    } else {
        st |= (WS_CAPTION | WS_THICKFRAME);
        Logger::Instance().Info(L"[Borderless Off] " + info.processName);
    }
    SetWindowLongPtrW(hwnd, GWL_STYLE, st);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    return true;
}

bool WindowEngine::QuickToggleTopMost(HWND hwnd) {
    WindowInfo info;
    if (!WindowInspector::Inspect(hwnd, info)) return false;
    if (WindowInspector::IsSystemProtectedWindow(hwnd, info.className, info.pid)) return false;

    const bool currentlyTopMost = (info.exStyle & WS_EX_TOPMOST) != 0;
    SetWindowPos(hwnd, currentlyTopMost ? HWND_NOTOPMOST : HWND_TOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    Logger::Instance().Info(std::wstring(currentlyTopMost ? L"[TopMost Off] " : L"[TopMost On] ") +
                            info.processName);
    return true;
}

bool WindowEngine::QuickMinimizeWindow(HWND hwnd) {
    WindowInfo info;
    if (!WindowInspector::Inspect(hwnd, info)) return false;
    if (WindowInspector::IsSystemProtectedWindow(hwnd, info.className, info.pid)) return false;
    ShowWindowAsync(hwnd, SW_MINIMIZE);
    Logger::Instance().Info(L"[Minimize] " + info.processName);
    return true;
}

bool WindowEngine::QuickCloseWindow(HWND hwnd) {
    WindowInfo info;
    if (!WindowInspector::Inspect(hwnd, info)) return false;
    if (WindowInspector::IsSystemProtectedWindow(hwnd, info.className, info.pid)) return false;
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
    Logger::Instance().Info(L"[Close] " + info.processName);
    return true;
}

bool WindowEngine::ProcessTimers() {
    const ULONGLONG now = GetTickCount64();
    std::vector<PendingDelayedAction> readyActions;
    std::vector<OpacityFadeTask> fadesCopy;

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        for (auto it = m_pendingActions.begin(); it != m_pendingActions.end();) {
            if (!IsWindow(it->hwnd)) {
                it = m_pendingActions.erase(it);
            } else if (now >= it->executeAtTick) {
                readyActions.push_back(*it);
                it = m_pendingActions.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = m_activeFades.begin(); it != m_activeFades.end();) {
            if (!IsWindow(it->hwnd)) {
                it = m_activeFades.erase(it);
                continue;
            }
            if (it->currentAlpha < it->targetAlpha) {
                it->currentAlpha = (std::min)(it->targetAlpha, it->currentAlpha + it->stepPerTick);
            } else if (it->currentAlpha > it->targetAlpha) {
                it->currentAlpha = (std::max)(it->targetAlpha, it->currentAlpha - it->stepPerTick);
            }
            fadesCopy.push_back(*it);
            if (it->currentAlpha == it->targetAlpha) {
                it = m_activeFades.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (const auto& act : readyActions) {
        WindowInfo info;
        if (WindowInspector::Inspect(act.hwnd, info) && info.isVisible) {
            ExecuteLifecycleAction(act.hwnd, info, act.rule, false);
        }
    }

    for (const auto& f : fadesCopy) {
        if (IsWindow(f.hwnd)) {
            SetLayeredWindowAttributes(f.hwnd, 0, static_cast<BYTE>(f.currentAlpha), LWA_ALPHA);
        }
    }

    return HasActiveTimers();
}

bool WindowEngine::HasActiveTimers() const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return !m_pendingActions.empty() || !m_activeFades.empty();
}

void WindowEngine::OnWinEvent(DWORD event, HWND hwnd, LONG idObject, LONG idChild) {
    if (!hwnd || idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;

    const uint64_t key = HwndToU64(hwnd);
    if (event == EVENT_OBJECT_DESTROY) {
        m_hookBridge.Detach(hwnd);
        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_tracked.erase(key);
        m_pendingActions.erase(
            std::remove_if(m_pendingActions.begin(), m_pendingActions.end(),
                           [hwnd](const PendingDelayedAction& a) { return a.hwnd == hwnd; }),
            m_pendingActions.end());
        m_activeFades.erase(
            std::remove_if(m_activeFades.begin(), m_activeFades.end(),
                           [hwnd](const OpacityFadeTask& f) { return f.hwnd == hwnd; }),
            m_activeFades.end());
        return;
    }

    if (!m_config.monitoringEnabled) return;
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return;

    if (event == EVENT_OBJECT_LOCATIONCHANGE) {
        std::wstring ruleId;
        RECT expectedRect{};
        ULONGLONG firstAppliedTick = 0;
        bool checkSnapback = false;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            auto it = m_tracked.find(key);
            if (it == m_tracked.end() || it->second.appliedRuleId.empty() || !it->second.hasTargetRect) {
                return;
            }
            const ULONGLONG now = GetTickCount64();
            if (now < it->second.suppressUntilTick) {
                return;
            }
            ruleId = it->second.appliedRuleId;
            expectedRect = it->second.lastTargetAdjustedRect;
            firstAppliedTick = it->second.firstAppliedTick;
            checkSnapback = true;
        }
        if (!checkSnapback) return;

        const Profile* prof = GetActiveProfile();
        const WindowRule* matchedRule = nullptr;
        if (prof) {
            for (const auto& r : prof->rules) {
                if (r.id == ruleId && r.enabled) {
                    matchedRule = &r;
                    break;
                }
            }
        }
        if (!matchedRule || matchedRule->action != WindowAction::Normal ||
            !matchedRule->aggressiveForce ||
            matchedRule->geometryMode == GeometryMode::StyleOnly ||
            matchedRule->geometryMode == GeometryMode::PositionOnlyPixels) {
            return;
        }

        const ULONGLONG nowTick = GetTickCount64();
        if (!matchedRule->lockTargetSize && (nowTick - firstAppliedTick > 4000)) {
            return;
        }

        RECT curRect{};
        if (!GetWindowRect(hwnd, &curRect)) return;
        const int expW = expectedRect.right - expectedRect.left;
        const int expH = expectedRect.bottom - expectedRect.top;
        const int curW = curRect.right - curRect.left;
        const int curH = curRect.bottom - curRect.top;

        if (std::abs(curW - expW) > 6 || std::abs(curH - expH) > 6) {
            if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0) {
                return;
            }

            bool allowReapply = false;
            {
                std::lock_guard<std::mutex> lock(m_stateMutex);
                auto it = m_tracked.find(key);
                if (it != m_tracked.end()) {
                    const ULONGLONG now = GetTickCount64();
                    if (now - it->second.lastReapplyWindowStart > 3000) {
                        it->second.lastReapplyWindowStart = now;
                        it->second.reapplyCountInBurst = 0;
                    }
                    if (it->second.reapplyCountInBurst < 5) {
                        ++it->second.reapplyCountInBurst;
                        it->second.suppressUntilTick = now + 200;
                        allowReapply = true;
                    }
                }
            }

            if (allowReapply) {
                WindowInfo info;
                if (WindowInspector::Inspect(hwnd, info) && !info.isMinimized) {
                    const auto monitors = MonitorManager::EnumerateMonitors();
                    TrackedWindow temp;
                    {
                        std::lock_guard<std::mutex> lock(m_stateMutex);
                        auto it = m_tracked.find(key);
                        if (it != m_tracked.end()) temp = it->second;
                    }
                    ApplyStyleAndPosition(hwnd, info, *matchedRule, temp, monitors);
                }
            }
        }
        return;
    }

    WindowInfo info;
    if (!WindowInspector::Inspect(hwnd, info)) return;
    if (WindowInspector::IsSystemProtectedWindow(hwnd, info.className, info.pid)) return;

    const WindowRule* rule = FindMatchingRule(info);
    if (!rule) return;

    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        auto it = m_tracked.find(key);
        if (it != m_tracked.end() && it->second.appliedRuleId == rule->id) {
            return;
        }
    }

    ApplyRuleToWindow(hwnd, *rule, false);
}

void WindowEngine::PeriodicMaintenance() {
    m_hookBridge.ReapDeadWindows();

    std::vector<uint64_t> dead;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        for (const auto& kv : m_tracked) {
            HWND h = reinterpret_cast<HWND>(static_cast<uintptr_t>(kv.first));
            if (!IsWindow(h)) {
                dead.push_back(kv.first);
            }
        }
        for (uint64_t k : dead) {
            m_tracked.erase(k);
        }
    }
}

} // namespace flexwin
