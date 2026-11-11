#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <shellapi.h>
#include <string>

#include "../core/Models.h"

namespace flexwin {

constexpr UINT WM_APP_TRAY = WM_APP + 101;

// Tray menu command IDs
constexpr UINT IDM_TRAY_SHOW = 40001;
constexpr UINT IDM_TRAY_APPLY_ALL = 40002;
constexpr UINT IDM_TRAY_TOGGLE_MONITOR = 40003;
constexpr UINT IDM_TRAY_EXPORT_PROFILE = 40004;
constexpr UINT IDM_TRAY_IMPORT_PROFILE = 40005;
constexpr UINT IDM_TRAY_STARTUP = 40006;
constexpr UINT IDM_TRAY_START_MIN = 40007;
constexpr UINT IDM_TRAY_CLOSE_TO_TRAY = 40008;
constexpr UINT IDM_TRAY_TOGGLE_HOOKS = 40009;
constexpr UINT IDM_TRAY_RUN_ADMIN = 40010;
constexpr UINT IDM_TRAY_LANG_TR = 40011;
constexpr UINT IDM_TRAY_LANG_EN = 40012;
constexpr UINT IDM_TRAY_EXIT = 40013;
constexpr UINT IDM_TRAY_PROFILE_BASE = 41000;

class TrayIcon {
public:
    TrayIcon() = default;
    ~TrayIcon();

    bool Create(HWND ownerHwnd, HICON icon, const std::wstring& tooltip);
    void Destroy();
    void UpdateTooltip(const std::wstring& tooltip);
    void ShowBalloon(const std::wstring& title, const std::wstring& text, DWORD infoFlags = NIIF_INFO);
    void ShowContextMenu(HWND ownerHwnd, const AppConfig& config, bool isElevated);

private:
    NOTIFYICONDATAW m_nid{};
    bool m_created = false;
};

} // namespace flexwin
