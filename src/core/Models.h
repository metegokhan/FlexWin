#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace flexwin {

enum class TitleMatchMode {
    Ignore = 0,
    Contains = 1,
    Exact = 2,
    StartsWith = 3,
    Regex = 4
};

enum class GeometryMode {
    Pixels = 0,
    Percent = 1,
    SizeOnlyPixels = 2,
    PositionOnlyPixels = 3,
    StyleOnly = 4
};

enum class MonitorTargetMode {
    WindowCurrent = 0,
    Primary = 1,
    Index = 2
};

enum class WindowAction {
    Normal = 0,
    AutoMinimize = 1,
    AutoClose = 2
};

struct HotkeyBinding {
    bool enabled = false;
    uint32_t modifiers = 0; // MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_WIN
    uint32_t vk = 0;        // Virtual key code (e.g. 'R')
};

struct WindowRule {
    std::wstring id;
    std::wstring name = L"Yeni Kural";
    bool enabled = true;

    // Matching criteria
    bool matchProcess = true;
    std::wstring processName;
    bool matchClass = false;
    std::wstring className;
    TitleMatchMode titleMode = TitleMatchMode::Ignore;
    std::wstring titlePattern;
    bool excludePopupsAndDialogs = true;
    int minSourceWidth = 120;
    int minSourceHeight = 80;

    // Target geometry
    GeometryMode geometryMode = GeometryMode::Percent;
    MonitorTargetMode monitorMode = MonitorTargetMode::WindowCurrent;
    int monitorIndex = 0;
    int x = 100;
    int y = 100;
    int width = 1280;
    int height = 800;
    double xPct = 0.0;
    double yPct = 0.0;
    double widthPct = 50.0;
    double heightPct = 100.0;

    // Lifecycle action & opacity
    WindowAction action = WindowAction::Normal;
    int actionDelayMs = 0; // Delay before AutoMinimize/AutoClose (ms)
    bool enableOpacity = false;
    int opacityPercent = 85; // 10..100
    bool smoothOpacityFade = true;

    // Enforcement & style
    bool makeResizable = true;
    bool borderless = false;
    bool alwaysOnTop = false;
    bool compensateDwmMargin = true;
    bool aggressiveForce = true;
    bool useHookDll = true;
    bool unlockHitTest = false;
    bool lockTargetSize = false;
    bool customMinMax = false;
    int customMinW = 320;
    int customMinH = 240;
    int customMaxW = 0;
    int customMaxH = 0;
    bool scaleChildControls = false;

    HotkeyBinding hotkey;
};

struct Profile {
    std::wstring name = L"Kurumsal Varsayılan";
    std::vector<WindowRule> rules;
};

struct GlobalHotkeys {
    HotkeyBinding unlockActiveWindow{true, 0x0003 /*MOD_ALT|MOD_CONTROL*/, 'R'};
    HotkeyBinding toggleBorderlessActive{true, 0x0003, 'B'};
    HotkeyBinding toggleTopMostActive{true, 0x0003, 'T'};
    HotkeyBinding applyAllRules{true, 0x0003, 'A'};
};

struct AppConfig {
    int schemaVersion = 1;
    std::wstring language = L"tr"; // "tr" or "en"
    bool monitoringEnabled = true;
    bool startMinimized = false;
    bool minimizeToTrayOnClose = true;
    bool runAtStartup = false;
    bool enableHookEngine = true;
    int activeProfileIndex = 0;
    std::vector<Profile> profiles;
    GlobalHotkeys hotkeys;
};

} // namespace flexwin
