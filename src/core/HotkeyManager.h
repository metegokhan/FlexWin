#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "Models.h"

namespace flexwin {

enum class HotkeyActionKind {
    UnlockActive = 1,
    ToggleBorderlessActive = 2,
    ToggleTopMostActive = 3,
    ApplyAllRules = 4,
    ApplyRuleById = 100
};

struct RegisteredHotkeyInfo {
    int id = 0;
    HotkeyActionKind kind = HotkeyActionKind::UnlockActive;
    std::wstring ruleId;
};

class HotkeyManager {
public:
    HotkeyManager() = default;
    ~HotkeyManager();

    void RegisterFromConfig(HWND targetHwnd, const AppConfig& config);
    void UnregisterAll();
    bool Lookup(int hotkeyId, RegisteredHotkeyInfo& outInfo) const;

    static std::wstring FormatHotkey(const HotkeyBinding& b);

private:
    HWND m_hwnd = nullptr;
    std::unordered_map<int, RegisteredHotkeyInfo> m_registered;
};

} // namespace flexwin
