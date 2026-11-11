#include "HotkeyManager.h"

#include <algorithm>
#include "Logger.h"

namespace flexwin {

HotkeyManager::~HotkeyManager() {
    UnregisterAll();
}

void HotkeyManager::UnregisterAll() {
    if (m_hwnd) {
        for (const auto& kv : m_registered) {
            UnregisterHotKey(m_hwnd, kv.first);
        }
    }
    m_registered.clear();
}

void HotkeyManager::RegisterFromConfig(HWND targetHwnd, const AppConfig& config) {
    UnregisterAll();
    m_hwnd = targetHwnd;
    if (!m_hwnd) return;

    auto tryReg = [&](int id, const HotkeyBinding& hb, HotkeyActionKind kind, const std::wstring& ruleId) {
        if (!hb.enabled || hb.vk == 0) return;
        const UINT mods = static_cast<UINT>(hb.modifiers) | MOD_NOREPEAT;
        if (RegisterHotKey(m_hwnd, id, mods, static_cast<UINT>(hb.vk))) {
            m_registered[id] = RegisteredHotkeyInfo{id, kind, ruleId};
        } else {
            Logger::Instance().Warn(L"Kısayol kaydedilemedi (" + FormatHotkey(hb) + L") — başka uygulama kullanıyor olabilir.");
        }
    };

    tryReg(1, config.hotkeys.unlockActiveWindow, HotkeyActionKind::UnlockActive, L"");
    tryReg(2, config.hotkeys.toggleBorderlessActive, HotkeyActionKind::ToggleBorderlessActive, L"");
    tryReg(3, config.hotkeys.toggleTopMostActive, HotkeyActionKind::ToggleTopMostActive, L"");
    tryReg(4, config.hotkeys.applyAllRules, HotkeyActionKind::ApplyAllRules, L"");

    if (!config.profiles.empty()) {
        int idx = std::clamp(config.activeProfileIndex, 0,
                             static_cast<int>(config.profiles.size()) - 1);
        int nextId = 100;
        for (const auto& rule : config.profiles[static_cast<size_t>(idx)].rules) {
            if (rule.enabled && rule.hotkey.enabled && rule.hotkey.vk != 0) {
                tryReg(nextId++, rule.hotkey, HotkeyActionKind::ApplyRuleById, rule.id);
            }
        }
    }
}

bool HotkeyManager::Lookup(int hotkeyId, RegisteredHotkeyInfo& outInfo) const {
    auto it = m_registered.find(hotkeyId);
    if (it == m_registered.end()) return false;
    outInfo = it->second;
    return true;
}

std::wstring HotkeyManager::FormatHotkey(const HotkeyBinding& b) {
    if (!b.enabled || b.vk == 0) return L"Yok";
    std::wstring s;
    if (b.modifiers & MOD_CONTROL) s += L"Ctrl+";
    if (b.modifiers & MOD_ALT) s += L"Alt+";
    if (b.modifiers & MOD_SHIFT) s += L"Shift+";
    if (b.modifiers & MOD_WIN) s += L"Win+";

    if ((b.vk >= '0' && b.vk <= '9') || (b.vk >= 'A' && b.vk <= 'Z')) {
        s.push_back(static_cast<wchar_t>(b.vk));
    } else if (b.vk >= VK_F1 && b.vk <= VK_F24) {
        s += L"F" + std::to_wstring(b.vk - VK_F1 + 1);
    } else {
        UINT scan = MapVirtualKeyW(b.vk, MAPVK_VK_TO_VSC);
        wchar_t keyName[64]{};
        if (GetKeyNameTextW(static_cast<LONG>(scan << 16), keyName, 64) > 0) {
            s += keyName;
        } else {
            s += L"VK_" + std::to_wstring(b.vk);
        }
    }
    return s;
}

} // namespace flexwin
