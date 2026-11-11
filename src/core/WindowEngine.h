#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "HookBridge.h"
#include "Models.h"
#include "MonitorManager.h"
#include "WindowInspector.h"

namespace flexwin {

struct ChildSnapshot {
    HWND child = nullptr;
    RECT origRect{};
};

struct WindowStateSnapshot {
    RECT origWindowRect{};
    LONG_PTR origStyle = 0;
    LONG_PTR origExStyle = 0;
    bool origHadLayered = false;
    BYTE origAlpha = 255;
    DWORD origLayeredFlags = LWA_ALPHA;
    int origClientW = 0;
    int origClientH = 0;
    std::vector<ChildSnapshot> children;
    bool captured = false;
};

struct TrackedWindow {
    std::wstring appliedRuleId;
    ULONGLONG firstAppliedTick = 0;
    ULONGLONG suppressUntilTick = 0;
    ULONGLONG lastReapplyWindowStart = 0;
    int reapplyCountInBurst = 0;
    RECT lastTargetAdjustedRect{};
    bool hasTargetRect = false;
    WindowStateSnapshot snapshot;
};

struct PendingDelayedAction {
    HWND hwnd = nullptr;
    WindowRule rule;
    ULONGLONG executeAtTick = 0;
};

struct OpacityFadeTask {
    HWND hwnd = nullptr;
    int currentAlpha = 255;
    int targetAlpha = 255;
    int stepPerTick = 12;
};

class WindowEngine {
public:
    WindowEngine() = default;
    ~WindowEngine();

    WindowEngine(const WindowEngine&) = delete;
    WindowEngine& operator=(const WindowEngine&) = delete;

    bool Initialize(HWND controllerHwnd, AppConfig config);
    void Shutdown();

    void UpdateConfig(const AppConfig& config);
    const AppConfig& GetConfig() const { return m_config; }

    void StartEventHooks();
    void StopEventHooks();
    bool AreEventHooksActive() const { return m_eventHookShow != nullptr; }

    // Rule matching & application
    static bool MatchesRule(const WindowRule& rule, const WindowInfo& info, bool ignoreEnabled = false);
    const WindowRule* FindMatchingRule(const WindowInfo& info) const;

    int ApplyActiveProfileToAllWindows();
    bool ApplyRuleToWindow(HWND hwnd, const WindowRule& rule, bool isManualTest = false);
    bool RevertWindow(HWND hwnd);
    void PeriodicMaintenance();
    bool ProcessTimers(); // Returns true while delayed actions or opacity fades remain active
    bool HasActiveTimers() const;

    // Quick ad-hoc interventions (for Hotkeys / Open Windows Browser)
    bool QuickUnlockWindow(HWND hwnd);
    bool QuickToggleBorderless(HWND hwnd);
    bool QuickToggleTopMost(HWND hwnd);
    bool QuickMinimizeWindow(HWND hwnd);
    bool QuickCloseWindow(HWND hwnd);
    bool SetWindowOpacity(HWND hwnd, int percent, bool smooth);

    HookBridge& GetHookBridge() { return m_hookBridge; }
    const HookBridge& GetHookBridge() const { return m_hookBridge; }

    void OnWinEvent(DWORD event, HWND hwnd, LONG idObject, LONG idChild);

private:
    const Profile* GetActiveProfile() const;
    void CaptureSnapshotIfNeeded(HWND hwnd, const WindowInfo& info, TrackedWindow& tracked, bool captureChildren);
    void ScaleDirectChildren(HWND hwnd, TrackedWindow& tracked);
    void ApplyStyleAndPosition(HWND hwnd, const WindowInfo& info, const WindowRule& rule,
                               TrackedWindow& tracked, const std::vector<MonitorInfoEx>& monitors);
    void ExecuteLifecycleAction(HWND hwnd, const WindowInfo& info, const WindowRule& rule, bool isManualTest);

    HWND m_controllerHwnd = nullptr;
    AppConfig m_config;
    HookBridge m_hookBridge;

    HWINEVENTHOOK m_eventHookShow = nullptr;
    HWINEVENTHOOK m_eventHookLoc = nullptr;
    HWINEVENTHOOK m_eventHookDestroy = nullptr;

    mutable std::mutex m_stateMutex;
    std::unordered_map<uint64_t, TrackedWindow> m_tracked;
    std::vector<PendingDelayedAction> m_pendingActions;
    std::vector<OpacityFadeTask> m_activeFades;
};

} // namespace flexwin
