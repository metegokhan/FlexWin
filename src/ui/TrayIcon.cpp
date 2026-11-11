#include "TrayIcon.h"

#include "../core/I18n.h"

namespace flexwin {

TrayIcon::~TrayIcon() {
    Destroy();
}

bool TrayIcon::Create(HWND ownerHwnd, HICON icon, const std::wstring& tooltip) {
    Destroy();
    m_nid = NOTIFYICONDATAW{};
    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = ownerHwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    m_nid.uCallbackMessage = WM_APP_TRAY;
    m_nid.hIcon = icon;
    wcsncpy_s(m_nid.szTip, tooltip.c_str(), _TRUNCATE);

    if (!Shell_NotifyIconW(NIM_ADD, &m_nid)) {
        return false;
    }
    m_nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &m_nid);
    m_created = true;
    return true;
}

void TrayIcon::Destroy() {
    if (m_created) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_created = false;
    }
}

void TrayIcon::UpdateTooltip(const std::wstring& tooltip) {
    if (!m_created) return;
    m_nid.uFlags = NIF_TIP | NIF_SHOWTIP;
    wcsncpy_s(m_nid.szTip, tooltip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::ShowBalloon(const std::wstring& title, const std::wstring& text, DWORD infoFlags) {
    if (!m_created) return;
    m_nid.uFlags = NIF_INFO;
    m_nid.dwInfoFlags = infoFlags;
    wcsncpy_s(m_nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(m_nid.szInfo, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &m_nid);
}

void TrayIcon::ShowContextMenu(HWND ownerHwnd, const AppConfig& config, bool isElevated) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, IDM_TRAY_SHOW, Tr(StrKey::TrayOpen));
    SetMenuDefaultItem(menu, IDM_TRAY_SHOW, FALSE);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING, IDM_TRAY_APPLY_ALL, Tr(StrKey::TrayApplyAll));
    AppendMenuW(menu, MF_STRING | (config.monitoringEnabled ? MF_CHECKED : 0),
                IDM_TRAY_TOGGLE_MONITOR, Tr(StrKey::TrayMonitoring));

    HMENU profMenu = CreatePopupMenu();
    if (profMenu) {
        for (size_t i = 0; i < config.profiles.size(); ++i) {
            UINT flags = MF_STRING | (static_cast<int>(i) == config.activeProfileIndex ? MF_CHECKED : 0);
            AppendMenuW(profMenu, flags, IDM_TRAY_PROFILE_BASE + static_cast<UINT>(i),
                        config.profiles[i].name.c_str());
        }
        AppendMenuW(profMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(profMenu, MF_STRING, IDM_TRAY_EXPORT_PROFILE, Tr(StrKey::TrayExport));
        AppendMenuW(profMenu, MF_STRING, IDM_TRAY_IMPORT_PROFILE, Tr(StrKey::TrayImport));
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(profMenu), Tr(StrKey::TrayProfiles));
    }

    HMENU langMenu = CreatePopupMenu();
    if (langMenu) {
        const bool isEn = (I18n::GetLanguage() == Language::En);
        AppendMenuW(langMenu, MF_STRING | (!isEn ? MF_CHECKED : 0), IDM_TRAY_LANG_TR, Tr(StrKey::TrayLangTr));
        AppendMenuW(langMenu, MF_STRING | (isEn ? MF_CHECKED : 0), IDM_TRAY_LANG_EN, Tr(StrKey::TrayLangEn));
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(langMenu), Tr(StrKey::TrayLanguage));
    }

    HMENU optMenu = CreatePopupMenu();
    if (optMenu) {
        AppendMenuW(optMenu, MF_STRING | (config.enableHookEngine ? MF_CHECKED : 0),
                    IDM_TRAY_TOGGLE_HOOKS, Tr(StrKey::TrayHookEngine));
        AppendMenuW(optMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(optMenu, MF_STRING | (config.runAtStartup ? MF_CHECKED : 0),
                    IDM_TRAY_STARTUP, Tr(StrKey::TrayStartup));
        AppendMenuW(optMenu, MF_STRING | (config.startMinimized ? MF_CHECKED : 0),
                    IDM_TRAY_START_MIN, Tr(StrKey::TrayStartMin));
        AppendMenuW(optMenu, MF_STRING | (config.minimizeToTrayOnClose ? MF_CHECKED : 0),
                    IDM_TRAY_CLOSE_TO_TRAY, Tr(StrKey::TrayCloseToTray));
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(optMenu), Tr(StrKey::TraySettings));
    }

    if (!isElevated) {
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, IDM_TRAY_RUN_ADMIN, Tr(StrKey::TrayRunAdmin));
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_TRAY_EXIT, Tr(StrKey::TrayExit));

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(ownerHwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, ownerHwnd, nullptr);
    PostMessageW(ownerHwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

} // namespace flexwin
