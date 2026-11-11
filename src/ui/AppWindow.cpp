#include "AppWindow.h"

#include <CommCtrl.h>
#include <commdlg.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>

#include "../core/ConfigManager.h"
#include "../core/I18n.h"
#include "../core/Logger.h"
#include "../core/StringUtil.h"
#include "Resource.h"

namespace flexwin {

namespace {

constexpr wchar_t kMainWindowClass[] = L"FlexWinMainWindowClass";
constexpr UINT WM_APP_LOG_UPDATE = WM_APP + 102;
constexpr UINT WM_APP_START_ANIM_TIMER = WM_APP + 104;
constexpr UINT_PTR kMaintTimerId = 1;
constexpr UINT_PTR kAnimTimerId = 2;
UINT g_msgShowMainUi = 0;

enum ControlId {
    IDC_COMBO_PROFILE = 1001,
    IDC_BTN_NEW_PROFILE,
    IDC_BTN_DEL_PROFILE,
    IDC_BTN_EXPORT_PROFILE,
    IDC_BTN_IMPORT_PROFILE,
    IDC_BTN_TOGGLE_MONITOR,
    IDC_BTN_APPLY_ALL,
    IDC_BTN_OPEN_WINDOWS,
    IDC_BTN_LOGS,
    IDC_BTN_LANG,
    IDC_BTN_ADMIN,

    IDC_LIST_RULES = 1020,
    IDC_BTN_ADD_RULE,
    IDC_BTN_DUP_RULE,
    IDC_BTN_DEL_RULE,
    IDC_BTN_MOVE_UP,
    IDC_BTN_MOVE_DOWN,

    IDC_PICKER_CTRL = 1040,
    IDC_BTN_PICK_OPEN,
    IDC_EDIT_RULE_NAME,
    IDC_CHK_ENABLED,
    IDC_CHK_PROCESS,
    IDC_EDIT_PROCESS,
    IDC_CHK_CLASS,
    IDC_EDIT_CLASS,
    IDC_COMBO_TITLE_MODE,
    IDC_EDIT_TITLE,
    IDC_CHK_EXCLUDE_POPUPS,
    IDC_EDIT_MIN_SRC_W,
    IDC_EDIT_MIN_SRC_H,

    IDC_COMBO_WINDOW_ACTION = 1060,
    IDC_EDIT_ACTION_DELAY,
    IDC_COMBO_GEOM_MODE,
    IDC_COMBO_MONITOR,
    IDC_EDIT_X,
    IDC_EDIT_Y,
    IDC_EDIT_W,
    IDC_EDIT_H,
    IDC_BTN_PRESET_LEFT,
    IDC_BTN_PRESET_RIGHT,
    IDC_BTN_PRESET_TOP,
    IDC_BTN_PRESET_BOTTOM,
    IDC_BTN_PRESET_FULL,
    IDC_BTN_PRESET_TL,
    IDC_BTN_PRESET_TR,
    IDC_BTN_PRESET_BL,
    IDC_BTN_PRESET_BR,
    IDC_BTN_PRESET_CENTER,
    IDC_MONITOR_PREVIEW,

    IDC_CHK_RESIZABLE = 1090,
    IDC_CHK_BORDERLESS,
    IDC_CHK_TOPMOST,
    IDC_CHK_DWM_MARGIN,
    IDC_CHK_AGGRESSIVE,
    IDC_CHK_HOOK_DLL,
    IDC_CHK_HITTEST,
    IDC_CHK_LOCK_SIZE,
    IDC_CHK_SCALE_CHILDREN,
    IDC_CHK_CUSTOM_MINMAX,
    IDC_EDIT_MIN_W,
    IDC_EDIT_MIN_H,
    IDC_EDIT_MAX_W,
    IDC_EDIT_MAX_H,
    IDC_CHK_OPACITY,
    IDC_SLIDER_OPACITY,
    IDC_CHK_SMOOTH_FADE,

    IDC_BTN_TEST_RULE = 1120,
    IDC_BTN_REVERT_LAST,
    IDC_BTN_SAVE,
    IDC_STATUS_BAR
};

int DpiScale(int px, UINT dpi) {
    return MulDiv(px, static_cast<int>(dpi), 96);
}

std::wstring GetEditText(HWND edit) {
    int len = GetWindowTextLengthW(edit);
    if (len <= 0) return {};
    std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(edit, &buf[0], len + 1);
    buf.resize(static_cast<size_t>(len));
    return buf;
}

int GetEditInt(HWND edit, int def = 0) {
    std::wstring s = TrimWide(GetEditText(edit));
    if (s.empty()) return def;
    wchar_t* endPtr = nullptr;
    long v = wcstol(s.c_str(), &endPtr, 10);
    return (endPtr == s.c_str()) ? def : static_cast<int>(v);
}

double GetEditDouble(HWND edit, double def = 0.0) {
    std::wstring s = TrimWide(GetEditText(edit));
    if (s.empty()) return def;
    for (wchar_t& c : s) {
        if (c == L',') c = L'.';
    }
    wchar_t* endPtr = nullptr;
    double v = wcstod(s.c_str(), &endPtr);
    return (endPtr == s.c_str() || !std::isfinite(v)) ? def : v;
}

void SetEditInt(HWND edit, int val) {
    SetWindowTextW(edit, std::to_wstring(val).c_str());
}

void SetEditDouble(HWND edit, double val) {
    wchar_t buf[32]{};
    if (std::floor(val) == val) {
        swprintf_s(buf, L"%.0f", val);
    } else {
        swprintf_s(buf, L"%.1f", val);
    }
    SetWindowTextW(edit, buf);
}

std::wstring FormatRuleSummary(const WindowRule& r) {
    if (r.action == WindowAction::AutoMinimize) return Tr(StrKey::SummaryMinimize);
    if (r.action == WindowAction::AutoClose) return Tr(StrKey::SummaryClose);

    std::wstring base;
    wchar_t buf[64]{};
    switch (r.geometryMode) {
    case GeometryMode::Percent:
        swprintf_s(buf, L"%%%.0f,%%%.0f (%%%.0fx%%%.0f)", r.xPct, r.yPct, r.widthPct, r.heightPct);
        base = buf;
        break;
    case GeometryMode::Pixels:
        swprintf_s(buf, L"%dx%d @%d,%d", r.width, r.height, r.x, r.y);
        base = buf;
        break;
    case GeometryMode::SizeOnlyPixels:
        swprintf_s(buf, L"%dx%d", r.width, r.height);
        base = buf;
        break;
    case GeometryMode::PositionOnlyPixels:
        swprintf_s(buf, L"@%d,%d", r.x, r.y);
        base = buf;
        break;
    case GeometryMode::StyleOnly:
        base = Tr(StrKey::SummaryStyleOnly);
        break;
    }
    if (r.enableOpacity) {
        base += L" [" + std::to_wstring(r.opacityPercent) + L"%]";
    }
    return base;
}

struct PromptDlgState {
    HWND hwnd = nullptr;
    HWND edit = nullptr;
    bool accepted = false;
    bool done = false;
    std::wstring text;
};

LRESULT CALLBACK PromptWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* st = reinterpret_cast<PromptDlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK && st) {
            st->text = TrimWide(GetEditText(st->edit));
            st->accepted = !st->text.empty();
            st->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL && st) {
            st->accepted = false;
            st->done = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        if (st) {
            st->accepted = false;
            st->done = true;
        }
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool ShowTextPrompt(HWND owner, HINSTANCE hInst, HFONT font, UINT dpi,
                    const wchar_t* title, const wchar_t* labelText, std::wstring& inOutText) {
    static bool registered = false;
    constexpr wchar_t kPromptCls[] = L"FlexWinPromptModal";
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = PromptWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        wc.lpszClassName = kPromptCls;
        RegisterClassExW(&wc);
        registered = true;
    }

    PromptDlgState st;
    st.text = inOutText;

    const int w = DpiScale(360, dpi);
    const int h = DpiScale(150, dpi);
    RECT orc{};
    GetWindowRect(owner, &orc);
    const int x = orc.left + ((orc.right - orc.left) - w) / 2;
    const int y = orc.top + ((orc.bottom - orc.top) - h) / 2;

    HWND dlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME, kPromptCls, title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, w, h, owner, nullptr, hInst, nullptr);
    if (!dlg) return false;
    st.hwnd = dlg;
    SetWindowLongPtrW(dlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));

    const int pad = DpiScale(12, dpi);
    const int cw = w - DpiScale(16, dpi);
    HWND lbl = CreateWindowExW(0, L"STATIC", labelText, WS_CHILD | WS_VISIBLE,
                               pad, pad, cw - pad * 2, DpiScale(20, dpi),
                               dlg, nullptr, hInst, nullptr);
    st.edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", inOutText.c_str(),
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                              pad, pad + DpiScale(24, dpi), cw - pad * 2, DpiScale(24, dpi),
                              dlg, nullptr, hInst, nullptr);
    HWND okBtn = CreateWindowExW(0, L"BUTTON", Tr(StrKey::BtnOk),
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                 cw - pad - DpiScale(170, dpi), pad + DpiScale(60, dpi),
                                 DpiScale(80, dpi), DpiScale(28, dpi),
                                 dlg, reinterpret_cast<HMENU>(IDOK), hInst, nullptr);
    HWND cancelBtn = CreateWindowExW(0, L"BUTTON", Tr(StrKey::BtnCancel),
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                     cw - pad - DpiScale(82, dpi), pad + DpiScale(60, dpi),
                                     DpiScale(80, dpi), DpiScale(28, dpi),
                                     dlg, reinterpret_cast<HMENU>(IDCANCEL), hInst, nullptr);
    for (HWND c : {lbl, st.edit, okBtn, cancelBtn}) {
        if (font) SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }

    EnableWindow(owner, FALSE);
    SetFocus(st.edit);
    SendMessageW(st.edit, EM_SETSEL, 0, -1);

    MSG msg{};
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            PostMessageW(dlg, WM_COMMAND, IDOK, 0);
            continue;
        }
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (st.accepted) {
        inOutText = st.text;
        return true;
    }
    return false;
}

std::wstring SanitizeFileName(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s) {
        if (wcschr(L"\\/:*?\"<>|", c)) out.push_back(L'_');
        else out.push_back(c);
    }
    return out.empty() ? L"FlexWinProfile" : out;
}

} // namespace

bool AppWindow::Register(HINSTANCE hInst) {
    g_msgShowMainUi = RegisterWindowMessageW(L"FlexWin_ShowMainUI");
    CrosshairPicker::Register(hInst);
    MonitorPreview::Register(hInst);
    OpenWindowsDialog::Register(hInst);
    LogDialog::Register(hInst);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = AppWindow::WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hIconSm = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON),
                                               IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kMainWindowClass;
    return RegisterClassExW(&wc) != 0;
}

bool AppWindow::Create(HINSTANCE hInst, AppConfig config, const std::wstring& configPath, bool startMinimized) {
    m_hInst = hInst;
    m_config = std::move(config);
    m_configPath = configPath;
    m_appIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
    I18n::SetLanguageCode(m_config.language);

    const int initW = 1180;
    const int initH = 795;
    m_hwnd = CreateWindowExW(
        0,
        kMainWindowClass,
        WindowInspector::IsCurrentProcessElevated() ? Tr(StrKey::AppTitleAdmin) : Tr(StrKey::AppTitle),
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, initW, initH,
        nullptr, nullptr, hInst, this);
    if (!m_hwnd) return false;

    if (g_msgShowMainUi != 0) {
        ChangeWindowMessageFilterEx(m_hwnd, g_msgShowMainUi, MSGFLT_ALLOW, nullptr);
    }

    m_dpi = GetDpiForWindow(m_hwnd);
    if (m_dpi == 0) m_dpi = 96;
    CreateFontsForDpi(m_dpi);
    ApplyFontsToChildren();

    const int scaledW = DpiScale(initW, m_dpi);
    const int scaledH = DpiScale(initH, m_dpi);
    RECT wa{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    const int cx = wa.left + ((wa.right - wa.left) - scaledW) / 2;
    const int cy = wa.top + ((wa.bottom - wa.top) - scaledH) / 2;
    SetWindowPos(m_hwnd, nullptr, cx, cy, scaledW, scaledH, SWP_NOZORDER);

    m_monitors = MonitorManager::EnumerateMonitors();
    m_engine.Initialize(m_hwnd, m_config);
    m_hotkeys.RegisterFromConfig(m_hwnd, m_config);

    m_tray.Create(m_hwnd, m_appIcon, Tr(StrKey::TrayTooltip));

    Logger::Instance().SetCallback([hwnd = m_hwnd](const LogEntry&) {
        if (IsWindow(hwnd)) {
            PostMessageW(hwnd, WM_APP_LOG_UPDATE, 0, 0);
        }
    });

    RefreshMonitorList();
    RefreshProfileCombo();
    RefreshRulesList(0);
    UpdateMonitorToggleButton();
    UpdateStatusBar();

    SetTimer(m_hwnd, kMaintTimerId, 2000, nullptr);

    if (!startMinimized && !m_config.startMinimized) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
    } else {
        ShowWindow(m_hwnd, SW_HIDE);
    }

    if (m_config.monitoringEnabled) {
        m_engine.ApplyActiveProfileToAllWindows();
    }

    return true;
}

void AppWindow::Shutdown() {
    m_exiting = true;
    Logger::Instance().SetCallback(nullptr);
    if (m_hwnd) {
        KillTimer(m_hwnd, kMaintTimerId);
        if (m_animTimerRunning) {
            KillTimer(m_hwnd, kAnimTimerId);
            m_animTimerRunning = false;
        }
    }
    m_openWinsDlg.Close();
    m_logDlg.Close();
    m_hotkeys.UnregisterAll();
    m_engine.Shutdown();
    m_tray.Destroy();
    if (m_font) { DeleteObject(m_font); m_font = nullptr; }
    if (m_fontBold) { DeleteObject(m_fontBold); m_fontBold = nullptr; }
}

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = static_cast<AppWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->m_hwnd = hwnd;
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    auto* self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void AppWindow::CreateFontsForDpi(UINT dpi) {
    if (m_font) DeleteObject(m_font);
    if (m_fontBold) DeleteObject(m_fontBold);

    const int height = -MulDiv(9, static_cast<int>(dpi), 72);
    m_font = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    m_fontBold = CreateFontW(height, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

void AppWindow::ApplyFontsToChildren() {
    EnumChildWindows(m_hwnd, [](HWND child, LPARAM lp) -> BOOL {
        SendMessageW(child, WM_SETFONT, lp, TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(m_font));

    if (m_btnTestRule && m_fontBold) {
        SendMessageW(m_btnTestRule, WM_SETFONT, reinterpret_cast<WPARAM>(m_fontBold), TRUE);
    }
    if (m_btnSave && m_fontBold) {
        SendMessageW(m_btnSave, WM_SETFONT, reinterpret_cast<WPARAM>(m_fontBold), TRUE);
    }
    m_picker.SetFont(m_fontBold ? m_fontBold : m_font);
    m_preview.SetFont(m_font);
}

void AppWindow::RepopulateTranslatedCombos() {
    m_populatingUi = true;

    const int titleSel = (std::max)(0, static_cast<int>(SendMessageW(m_comboTitleMode, CB_GETCURSEL, 0, 0)));
    SendMessageW(m_comboTitleMode, CB_RESETCONTENT, 0, 0);
    SendMessageW(m_comboTitleMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::TitleIgnore)));
    SendMessageW(m_comboTitleMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::TitleContains)));
    SendMessageW(m_comboTitleMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::TitleExact)));
    SendMessageW(m_comboTitleMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::TitleStartsWith)));
    SendMessageW(m_comboTitleMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::TitleRegex)));
    SendMessageW(m_comboTitleMode, CB_SETCURSEL, static_cast<WPARAM>(titleSel), 0);

    const int actSel = (std::max)(0, static_cast<int>(SendMessageW(m_comboWindowAction, CB_GETCURSEL, 0, 0)));
    SendMessageW(m_comboWindowAction, CB_RESETCONTENT, 0, 0);
    SendMessageW(m_comboWindowAction, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::ActNormal)));
    SendMessageW(m_comboWindowAction, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::ActMinimize)));
    SendMessageW(m_comboWindowAction, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::ActClose)));
    SendMessageW(m_comboWindowAction, CB_SETCURSEL, static_cast<WPARAM>(actSel), 0);

    const int geomSel = (std::max)(0, static_cast<int>(SendMessageW(m_comboGeomMode, CB_GETCURSEL, 0, 0)));
    SendMessageW(m_comboGeomMode, CB_RESETCONTENT, 0, 0);
    SendMessageW(m_comboGeomMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::GeomPixels)));
    SendMessageW(m_comboGeomMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::GeomPercent)));
    SendMessageW(m_comboGeomMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::GeomSizeOnly)));
    SendMessageW(m_comboGeomMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::GeomPosOnly)));
    SendMessageW(m_comboGeomMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::GeomStyleOnly)));
    SendMessageW(m_comboGeomMode, CB_SETCURSEL, static_cast<WPARAM>(geomSel), 0);

    m_populatingUi = false;
}

void AppWindow::CreateControls() {
    auto mk = [&](const wchar_t* cls, const wchar_t* text, DWORD style, DWORD exStyle, int id) {
        return CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style,
                               0, 0, 50, 24, m_hwnd,
                               id ? reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)) : nullptr,
                               m_hInst, nullptr);
    };

    const bool elevated = WindowInspector::IsCurrentProcessElevated();

    // Top bar
    m_lblProfile = mk(L"STATIC", Tr(StrKey::LblProfile), SS_CENTERIMAGE, 0, 0);
    m_comboProfile = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, IDC_COMBO_PROFILE);
    m_btnNewProfile = mk(L"BUTTON", Tr(StrKey::BtnNewProfile), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_NEW_PROFILE);
    m_btnDelProfile = mk(L"BUTTON", Tr(StrKey::BtnDelProfile), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_DEL_PROFILE);
    m_btnExportProfile = mk(L"BUTTON", Tr(StrKey::BtnExportProfile), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_EXPORT_PROFILE);
    m_btnImportProfile = mk(L"BUTTON", Tr(StrKey::BtnImportProfile), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_IMPORT_PROFILE);
    m_btnToggleMonitor = mk(L"BUTTON", Tr(StrKey::BtnMonitorOn), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_TOGGLE_MONITOR);
    m_btnApplyAll = mk(L"BUTTON", Tr(StrKey::BtnApplyAll), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_APPLY_ALL);
    m_btnOpenWindows = mk(L"BUTTON", Tr(StrKey::BtnOpenWindows), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_OPEN_WINDOWS);
    m_btnLogs = mk(L"BUTTON", Tr(StrKey::BtnLogs), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_LOGS);
    m_btnLang = mk(L"BUTTON", Tr(StrKey::LangToggleBtn), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_LANG);
    m_btnAdmin = mk(L"BUTTON", elevated ? Tr(StrKey::BtnAdminActive) : Tr(StrKey::BtnAdmin),
                    BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_ADMIN);
    if (elevated) {
        EnableWindow(m_btnAdmin, FALSE);
    }

    // Left panel
    m_grpRules = mk(L"BUTTON", Tr(StrKey::GrpRules), BS_GROUPBOX, 0, 0);
    m_listRules = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 100, 100, m_hwnd, reinterpret_cast<HMENU>(IDC_LIST_RULES), m_hInst, nullptr);
    ListView_SetExtendedListViewStyle(
        m_listRules, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    auto addCol = [&](int idx, const wchar_t* text, int w) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = const_cast<wchar_t*>(text);
        col.cx = DpiScale(w, m_dpi);
        col.iSubItem = idx;
        ListView_InsertColumn(m_listRules, idx, &col);
    };
    addCol(0, Tr(StrKey::ColRule), 135);
    addCol(1, Tr(StrKey::ColTarget), 102);
    addCol(2, Tr(StrKey::ColModeAction), 106);

    m_btnAddRule = mk(L"BUTTON", Tr(StrKey::BtnAddRule), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_ADD_RULE);
    m_btnDupRule = mk(L"BUTTON", Tr(StrKey::BtnDupRule), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_DUP_RULE);
    m_btnDelRule = mk(L"BUTTON", Tr(StrKey::BtnDelRule), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_DEL_RULE);
    m_btnMoveUp = mk(L"BUTTON", Tr(StrKey::BtnMoveUp), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_MOVE_UP);
    m_btnMoveDown = mk(L"BUTTON", Tr(StrKey::BtnMoveDown), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_MOVE_DOWN);
    m_lblHotkeyHint = mk(L"STATIC", Tr(StrKey::LblHotkeyHint), 0, 0, 0);

    // Right panel - Section 1: Match
    m_grpMatch = mk(L"BUTTON", Tr(StrKey::GrpMatch), BS_GROUPBOX, 0, 0);
    m_picker.Create(m_hwnd, m_hInst, 0, 0, 200, 34, IDC_PICKER_CTRL);
    m_picker.SetOnHover([this](HWND hovered) {
        if (!hovered) {
            SetWindowTextW(m_lblPickedInfo, Tr(StrKey::PickerDragging));
            return;
        }
        WindowInfo info;
        if (WindowInspector::Inspect(hovered, info)) {
            wchar_t msg[512]{};
            swprintf_s(msg, L"%s | %s | %ldx%ld | %s | %s",
                       info.processName.c_str(), info.className.c_str(),
                       info.visualRect.right - info.visualRect.left,
                       info.visualRect.bottom - info.visualRect.top,
                       info.isResizableStyle ? Tr(StrKey::OwResizable) : Tr(StrKey::OwLocked),
                       info.is32Bit ? L"x86" : L"x64");
            SetWindowTextW(m_lblPickedInfo, msg);
        }
    });
    m_picker.SetOnPicked([this](HWND picked) {
        if (!picked) {
            SetWindowTextW(m_lblPickedInfo, Tr(StrKey::PickedCancelled));
            return;
        }
        WindowInfo info;
        if (WindowInspector::Inspect(picked, info)) {
            if (WindowInspector::IsSystemProtectedWindow(picked, info.className, info.pid)) {
                SetWindowTextW(m_lblPickedInfo, Tr(StrKey::PickedProtected));
                return;
            }
            const bool createNew = (GetSelectedRule() == nullptr);
            PopulateRuleFromWindow(info, createNew);
        }
    });

    m_btnPickFromOpen = mk(L"BUTTON", Tr(StrKey::BtnPickOpen), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_PICK_OPEN);
    m_lblPickedInfo = mk(L"STATIC", Tr(StrKey::PickedInfoDefault), SS_ENDELLIPSIS, 0, 0);

    m_lblRuleName = mk(L"STATIC", Tr(StrKey::LblRuleName), SS_CENTERIMAGE, 0, 0);
    m_editRuleName = mk(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_RULE_NAME);
    m_chkEnabled = mk(L"BUTTON", Tr(StrKey::ChkEnabled), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_ENABLED);

    m_chkProcess = mk(L"BUTTON", Tr(StrKey::ChkProcess), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_PROCESS);
    m_editProcess = mk(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_PROCESS);
    m_chkClass = mk(L"BUTTON", Tr(StrKey::ChkClass), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_CLASS);
    m_editClass = mk(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_CLASS);

    m_lblTitle = mk(L"STATIC", Tr(StrKey::LblTitle), SS_CENTERIMAGE, 0, 0);
    m_comboTitleMode = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, IDC_COMBO_TITLE_MODE);
    m_editTitle = mk(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_TITLE);

    m_chkExcludePopups = mk(L"BUTTON", Tr(StrKey::ChkExcludePopups), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_EXCLUDE_POPUPS);
    m_lblMinSrc = mk(L"STATIC", Tr(StrKey::LblMinSrc), SS_CENTERIMAGE, 0, 0);
    m_editMinSrcW = mk(L"EDIT", L"120", ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_MIN_SRC_W);
    m_editMinSrcH = mk(L"EDIT", L"80", ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_MIN_SRC_H);

    // Right panel - Section 2: Action, Geometry & Preview
    m_grpGeom = mk(L"BUTTON", Tr(StrKey::GrpGeom), BS_GROUPBOX, 0, 0);
    m_lblWindowAction = mk(L"STATIC", Tr(StrKey::LblWindowAction), SS_CENTERIMAGE, 0, 0);
    m_comboWindowAction = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, IDC_COMBO_WINDOW_ACTION);
    m_lblActionDelay = mk(L"STATIC", Tr(StrKey::LblDelayMs), SS_CENTERIMAGE, 0, 0);
    m_editActionDelay = mk(L"EDIT", L"0", ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_ACTION_DELAY);

    m_lblGeomMode = mk(L"STATIC", Tr(StrKey::LblGeomMode), SS_CENTERIMAGE, 0, 0);
    m_comboGeomMode = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, IDC_COMBO_GEOM_MODE);

    m_lblMonitor = mk(L"STATIC", Tr(StrKey::LblMonitor), SS_CENTERIMAGE, 0, 0);
    m_comboMonitor = mk(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 0, IDC_COMBO_MONITOR);

    m_lblX = mk(L"STATIC", Tr(StrKey::LblXPct), SS_CENTERIMAGE, 0, 0);
    m_editX = mk(L"EDIT", L"0", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_X);
    m_lblY = mk(L"STATIC", Tr(StrKey::LblYPct), SS_CENTERIMAGE, 0, 0);
    m_editY = mk(L"EDIT", L"0", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_Y);
    m_lblW = mk(L"STATIC", Tr(StrKey::LblWPct), SS_CENTERIMAGE, 0, 0);
    m_editW = mk(L"EDIT", L"50", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_W);
    m_lblH = mk(L"STATIC", Tr(StrKey::LblHPct), SS_CENTERIMAGE, 0, 0);
    m_editH = mk(L"EDIT", L"100", ES_AUTOHSCROLL | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_H);

    m_btnPresetLeft = mk(L"BUTTON", Tr(StrKey::PresetLeft), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_PRESET_LEFT);
    m_btnPresetRight = mk(L"BUTTON", Tr(StrKey::PresetRight), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_PRESET_RIGHT);
    m_btnPresetTop = mk(L"BUTTON", Tr(StrKey::PresetTop), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_PRESET_TOP);
    m_btnPresetBottom = mk(L"BUTTON", Tr(StrKey::PresetBottom), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_PRESET_BOTTOM);
    m_btnPresetFull = mk(L"BUTTON", Tr(StrKey::PresetFull), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_PRESET_FULL);
    m_btnPresetTL = mk(L"BUTTON", Tr(StrKey::PresetTL), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_PRESET_TL);
    m_btnPresetTR = mk(L"BUTTON", Tr(StrKey::PresetTR), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_PRESET_TR);
    m_btnPresetBL = mk(L"BUTTON", Tr(StrKey::PresetBL), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_PRESET_BL);
    m_btnPresetBR = mk(L"BUTTON", Tr(StrKey::PresetBR), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_PRESET_BR);
    m_btnPresetCenter = mk(L"BUTTON", Tr(StrKey::PresetCenter), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_PRESET_CENTER);

    m_preview.Create(m_hwnd, m_hInst, 0, 0, 200, 150, IDC_MONITOR_PREVIEW);
    m_preview.SetOnMonitorClicked([this](int monIdx) {
        WindowRule* r = GetSelectedRule();
        if (!r) return;
        r->monitorMode = MonitorTargetMode::Index;
        r->monitorIndex = monIdx;
        m_populatingUi = true;
        SendMessageW(m_comboMonitor, CB_SETCURSEL, static_cast<WPARAM>(2 + monIdx), 0);
        m_populatingUi = false;
        UpdatePreview();
    });

    // Right panel - Section 3: Enforcement, Opacity & Options
    m_grpOpts = mk(L"BUTTON", Tr(StrKey::GrpOpts), BS_GROUPBOX, 0, 0);
    m_chkResizable = mk(L"BUTTON", Tr(StrKey::ChkResizable), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_RESIZABLE);
    m_chkBorderless = mk(L"BUTTON", Tr(StrKey::ChkBorderless), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_BORDERLESS);
    m_chkTopMost = mk(L"BUTTON", Tr(StrKey::ChkTopMost), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_TOPMOST);
    m_chkDwmMargin = mk(L"BUTTON", Tr(StrKey::ChkDwmMargin), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_DWM_MARGIN);

    m_chkAggressive = mk(L"BUTTON", Tr(StrKey::ChkAggressive), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_AGGRESSIVE);
    m_chkHookDll = mk(L"BUTTON", Tr(StrKey::ChkHookDll), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_HOOK_DLL);
    m_chkHitTest = mk(L"BUTTON", Tr(StrKey::ChkHitTest), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_HITTEST);
    m_chkLockSize = mk(L"BUTTON", Tr(StrKey::ChkLockSize), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_LOCK_SIZE);

    m_chkScaleChildren = mk(L"BUTTON", Tr(StrKey::ChkScaleChildren), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_SCALE_CHILDREN);
    m_chkCustomMinMax = mk(L"BUTTON", Tr(StrKey::ChkCustomMinMax), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_CUSTOM_MINMAX);
    m_editMinW = mk(L"EDIT", L"320", ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_MIN_W);
    m_editMinH = mk(L"EDIT", L"240", ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_MIN_H);
    m_editMaxW = mk(L"EDIT", L"0", ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_MAX_W);
    m_editMaxH = mk(L"EDIT", L"0", ES_AUTOHSCROLL | ES_NUMBER | WS_TABSTOP, WS_EX_CLIENTEDGE, IDC_EDIT_MAX_H);

    m_chkOpacity = mk(L"BUTTON", Tr(StrKey::ChkOpacity), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_OPACITY);
    m_sliderOpacity = CreateWindowExW(
        0, TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_AUTOTICKS,
        0, 0, 150, 24, m_hwnd, reinterpret_cast<HMENU>(IDC_SLIDER_OPACITY), m_hInst, nullptr);
    SendMessageW(m_sliderOpacity, TBM_SETRANGE, TRUE, MAKELPARAM(10, 100));
    SendMessageW(m_sliderOpacity, TBM_SETTICFREQ, 10, 0);
    SendMessageW(m_sliderOpacity, TBM_SETPOS, TRUE, 85);
    m_lblOpacityVal = mk(L"STATIC", L"85%", SS_CENTERIMAGE, 0, 0);
    m_chkSmoothFade = mk(L"BUTTON", Tr(StrKey::ChkSmoothFade), BS_AUTOCHECKBOX | WS_TABSTOP, 0, IDC_CHK_SMOOTH_FADE);

    m_btnTestRule = mk(L"BUTTON", Tr(StrKey::BtnTestRule), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_TEST_RULE);
    m_btnRevertLast = mk(L"BUTTON", Tr(StrKey::BtnRevertLast), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_REVERT_LAST);
    m_btnSave = mk(L"BUTTON", Tr(StrKey::BtnSave), BS_PUSHBUTTON | WS_TABSTOP, 0, IDC_BTN_SAVE);

    m_statusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"",
                                  WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                  0, 0, 0, 0, m_hwnd,
                                  reinterpret_cast<HMENU>(IDC_STATUS_BAR), m_hInst, nullptr);

    RepopulateTranslatedCombos();
}

void AppWindow::LayoutControls(int cw, int ch) {
    if (!m_statusBar) return;
    auto S = [this](int px) { return DpiScale(px, m_dpi); };

    SendMessageW(m_statusBar, WM_SIZE, 0, 0);
    RECT sbRc{};
    GetWindowRect(m_statusBar, &sbRc);
    const int sbH = sbRc.bottom - sbRc.top;

    int sbParts[3] = {S(250), S(560), -1};
    SendMessageW(m_statusBar, SB_SETPARTS, 3, reinterpret_cast<LPARAM>(sbParts));

    const int pad = S(10);
    const int gap = S(5);
    const int rowH = S(23);
    const int btnH = S(27);

    // Top bar
    int tx = pad;
    const int ty = pad;
    MoveWindow(m_lblProfile, tx, ty + S(2), S(42), rowH, TRUE); tx += S(44);
    MoveWindow(m_comboProfile, tx, ty, S(150), S(200), TRUE); tx += S(150) + gap;
    MoveWindow(m_btnNewProfile, tx, ty, S(64), btnH, TRUE); tx += S(64) + gap;
    MoveWindow(m_btnDelProfile, tx, ty, S(40), btnH, TRUE); tx += S(40) + gap;
    MoveWindow(m_btnExportProfile, tx, ty, S(78), btnH, TRUE); tx += S(78) + gap;
    MoveWindow(m_btnImportProfile, tx, ty, S(72), btnH, TRUE); tx += S(72) + gap * 2;
    MoveWindow(m_btnToggleMonitor, tx, ty, S(108), btnH, TRUE); tx += S(108) + gap;
    MoveWindow(m_btnApplyAll, tx, ty, S(106), btnH, TRUE); tx += S(106) + gap;
    MoveWindow(m_btnOpenWindows, tx, ty, S(112), btnH, TRUE); tx += S(112) + gap;
    MoveWindow(m_btnLogs, tx, ty, S(60), btnH, TRUE);

    const int adminW = S(98);
    const int langW = S(58);
    MoveWindow(m_btnAdmin, cw - pad - adminW, ty, adminW, btnH, TRUE);
    MoveWindow(m_btnLang, cw - pad - adminW - gap - langW, ty, langW, btnH, TRUE);

    const int bodyTop = ty + btnH + pad;
    const int bodyBottom = ch - sbH - pad;
    const int bodyH = (std::max)(S(320), bodyBottom - bodyTop);

    // Left panel
    const int leftW = S(368);
    MoveWindow(m_grpRules, pad, bodyTop, leftW, bodyH, TRUE);

    const int lPad = pad + S(10);
    const int lInnerW = leftW - S(20);
    const int hintH = S(32);
    const int listBottomBtnY = bodyTop + bodyH - S(10) - hintH - gap - btnH;
    const int listTop = bodyTop + S(20);
    const int listH = (std::max)(S(120), listBottomBtnY - listTop - gap);

    MoveWindow(m_listRules, lPad, listTop, lInnerW, listH, TRUE);

    const int bw = (lInnerW - gap * 4) / 5;
    int bx = lPad;
    MoveWindow(m_btnAddRule, bx, listBottomBtnY, bw, btnH, TRUE); bx += bw + gap;
    MoveWindow(m_btnDupRule, bx, listBottomBtnY, bw, btnH, TRUE); bx += bw + gap;
    MoveWindow(m_btnDelRule, bx, listBottomBtnY, bw, btnH, TRUE); bx += bw + gap;
    MoveWindow(m_btnMoveUp, bx, listBottomBtnY, bw, btnH, TRUE); bx += bw + gap;
    MoveWindow(m_btnMoveDown, bx, listBottomBtnY, lInnerW - (bw + gap) * 4, btnH, TRUE);

    MoveWindow(m_lblHotkeyHint, lPad, listBottomBtnY + btnH + gap, lInnerW, hintH, TRUE);

    // Right panel
    const int rightX = pad + leftW + pad;
    const int rightW = (std::max)(S(480), cw - rightX - pad);
    const int rPad = rightX + S(10);
    const int rInnerW = rightW - S(20);

    // Section 1: Match (~186)
    const int sec1H = S(186);
    MoveWindow(m_grpMatch, rightX, bodyTop, rightW, sec1H, TRUE);
    int ry = bodyTop + S(20);
    const int pickOpenW = S(130);
    if (HWND pickerHwnd = m_picker.GetHwnd()) {
        MoveWindow(pickerHwnd, rPad, ry, rInnerW - pickOpenW - gap, S(31), TRUE);
    }
    MoveWindow(m_btnPickFromOpen, rPad + rInnerW - pickOpenW, ry, pickOpenW, S(31), TRUE);
    ry += S(34);
    MoveWindow(m_lblPickedInfo, rPad, ry, rInnerW, S(18), TRUE);
    ry += S(21);

    const int halfW = (rInnerW - gap) / 2;
    MoveWindow(m_lblRuleName, rPad, ry + S(2), S(68), rowH, TRUE);
    MoveWindow(m_editRuleName, rPad + S(70), ry, rInnerW - S(70) - S(105) - gap, rowH, TRUE);
    MoveWindow(m_chkEnabled, rPad + rInnerW - S(102), ry + S(2), S(102), rowH, TRUE);
    ry += rowH + gap;

    MoveWindow(m_chkProcess, rPad, ry + S(2), S(96), rowH, TRUE);
    MoveWindow(m_editProcess, rPad + S(98), ry, halfW - S(98), rowH, TRUE);
    const int col2X = rPad + halfW + gap;
    MoveWindow(m_chkClass, col2X, ry + S(2), S(96), rowH, TRUE);
    MoveWindow(m_editClass, col2X + S(98), ry, rInnerW - halfW - gap - S(98), rowH, TRUE);
    ry += rowH + gap;

    MoveWindow(m_lblTitle, rPad, ry + S(2), S(54), rowH, TRUE);
    MoveWindow(m_comboTitleMode, rPad + S(56), ry, S(115), S(160), TRUE);
    MoveWindow(m_editTitle, rPad + S(177), ry, rInnerW - S(177), rowH, TRUE);
    ry += rowH + gap;

    MoveWindow(m_chkExcludePopups, rPad, ry + S(2), S(330), rowH, TRUE);
    const int minSrcX = rPad + rInnerW - S(230);
    MoveWindow(m_lblMinSrc, minSrcX, ry + S(2), S(118), rowH, TRUE);
    MoveWindow(m_editMinSrcW, minSrcX + S(120), ry, S(52), rowH, TRUE);
    MoveWindow(m_editMinSrcH, minSrcX + S(176), ry, S(52), rowH, TRUE);

    // Section 2: Action, Geometry & Preview (~250)
    const int sec2Y = bodyTop + sec1H + gap;
    const int sec2H = S(250);
    MoveWindow(m_grpGeom, rightX, sec2Y, rightW, sec2H, TRUE);

    const int previewW = (std::max)(S(200), (rInnerW * 38) / 100);
    const int geomFormW = rInnerW - previewW - pad;
    int gy = sec2Y + S(21);

    const int delayEditW = S(58);
    const int delayLblW = S(82);
    const int actionComboW = (std::max)(S(140), geomFormW - S(100) - delayLblW - delayEditW - gap * 2);
    MoveWindow(m_lblWindowAction, rPad, gy + S(2), S(98), rowH, TRUE);
    MoveWindow(m_comboWindowAction, rPad + S(100), gy, actionComboW, S(160), TRUE);
    MoveWindow(m_lblActionDelay, rPad + S(100) + actionComboW + gap, gy + S(2), delayLblW, rowH, TRUE);
    MoveWindow(m_editActionDelay, rPad + geomFormW - delayEditW, gy, delayEditW, rowH, TRUE);
    gy += rowH + gap;

    MoveWindow(m_lblGeomMode, rPad, gy + S(2), S(98), rowH, TRUE);
    MoveWindow(m_comboGeomMode, rPad + S(100), gy, geomFormW - S(100), S(180), TRUE);
    gy += rowH + gap;

    MoveWindow(m_lblMonitor, rPad, gy + S(2), S(98), rowH, TRUE);
    MoveWindow(m_comboMonitor, rPad + S(100), gy, geomFormW - S(100), S(180), TRUE);
    gy += rowH + gap;

    const int qW = (geomFormW - gap * 3) / 4;
    const int lblW = S(42);
    const int valW = (std::max)(S(36), qW - lblW);
    int gx = rPad;
    MoveWindow(m_lblX, gx, gy + S(2), lblW, rowH, TRUE);
    MoveWindow(m_editX, gx + lblW, gy, valW, rowH, TRUE); gx += qW + gap;
    MoveWindow(m_lblY, gx, gy + S(2), lblW, rowH, TRUE);
    MoveWindow(m_editY, gx + lblW, gy, valW, rowH, TRUE); gx += qW + gap;
    MoveWindow(m_lblW, gx, gy + S(2), lblW + S(10), rowH, TRUE);
    MoveWindow(m_editW, gx + lblW + S(10), gy, valW - S(10), rowH, TRUE); gx += qW + gap;
    MoveWindow(m_lblH, gx, gy + S(2), lblW + S(10), rowH, TRUE);
    MoveWindow(m_editH, gx + lblW + S(10), gy, (rPad + geomFormW) - (gx + lblW + S(10)), rowH, TRUE);
    gy += rowH + gap * 2;

    const int pBtnW = (geomFormW - gap * 4) / 5;
    const int pBtnH = S(26);
    int px = rPad;
    MoveWindow(m_btnPresetLeft, px, gy, pBtnW, pBtnH, TRUE); px += pBtnW + gap;
    MoveWindow(m_btnPresetRight, px, gy, pBtnW, pBtnH, TRUE); px += pBtnW + gap;
    MoveWindow(m_btnPresetTop, px, gy, pBtnW, pBtnH, TRUE); px += pBtnW + gap;
    MoveWindow(m_btnPresetBottom, px, gy, pBtnW, pBtnH, TRUE); px += pBtnW + gap;
    MoveWindow(m_btnPresetFull, px, gy, geomFormW - (pBtnW + gap) * 4, pBtnH, TRUE);
    gy += pBtnH + gap;

    px = rPad;
    MoveWindow(m_btnPresetTL, px, gy, pBtnW, pBtnH, TRUE); px += pBtnW + gap;
    MoveWindow(m_btnPresetTR, px, gy, pBtnW, pBtnH, TRUE); px += pBtnW + gap;
    MoveWindow(m_btnPresetBL, px, gy, pBtnW, pBtnH, TRUE); px += pBtnW + gap;
    MoveWindow(m_btnPresetBR, px, gy, pBtnW, pBtnH, TRUE); px += pBtnW + gap;
    MoveWindow(m_btnPresetCenter, px, gy, geomFormW - (pBtnW + gap) * 4, pBtnH, TRUE);

    if (HWND prevHwnd = m_preview.GetHwnd()) {
        MoveWindow(prevHwnd, rPad + geomFormW + pad, sec2Y + S(21), previewW, sec2H - S(30), TRUE);
    }

    // Section 3: Enforcement, Opacity & Options + Action buttons
    const int actionH = S(32);
    const int sec3Y = sec2Y + sec2H + gap;
    const int sec3H = (std::max)(S(172), bodyTop + bodyH - sec3Y - actionH - gap);
    MoveWindow(m_grpOpts, rightX, sec3Y, rightW, sec3H, TRUE);

    int oy = sec3Y + S(20);
    const int optHalf = (rInnerW - gap) / 2;
    const int chkH = S(22);
    MoveWindow(m_chkResizable, rPad, oy, optHalf, chkH, TRUE);
    MoveWindow(m_chkAggressive, rPad + optHalf + gap, oy, rInnerW - optHalf - gap, chkH, TRUE);
    oy += chkH + S(2);
    MoveWindow(m_chkBorderless, rPad, oy, optHalf, chkH, TRUE);
    MoveWindow(m_chkHookDll, rPad + optHalf + gap, oy, rInnerW - optHalf - gap, chkH, TRUE);
    oy += chkH + S(2);
    MoveWindow(m_chkTopMost, rPad, oy, optHalf, chkH, TRUE);
    MoveWindow(m_chkHitTest, rPad + optHalf + gap, oy, rInnerW - optHalf - gap, chkH, TRUE);
    oy += chkH + S(2);
    MoveWindow(m_chkDwmMargin, rPad, oy, optHalf, chkH, TRUE);
    MoveWindow(m_chkLockSize, rPad + optHalf + gap, oy, rInnerW - optHalf - gap, chkH, TRUE);
    oy += chkH + S(4);

    MoveWindow(m_chkScaleChildren, rPad, oy, S(268), chkH, TRUE);
    const int mmX = rPad + S(274);
    MoveWindow(m_chkCustomMinMax, mmX, oy, S(150), chkH, TRUE);
    int ex = mmX + S(152);
    const int mmEditW = (std::max)(S(36), (rPad + rInnerW - ex - gap * 3) / 4);
    MoveWindow(m_editMinW, ex, oy, mmEditW, rowH, TRUE); ex += mmEditW + gap;
    MoveWindow(m_editMinH, ex, oy, mmEditW, rowH, TRUE); ex += mmEditW + gap;
    MoveWindow(m_editMaxW, ex, oy, mmEditW, rowH, TRUE); ex += mmEditW + gap;
    MoveWindow(m_editMaxH, ex, oy, (rPad + rInnerW) - ex, rowH, TRUE);
    oy += rowH + S(5);

    // Gradual Opacity Row
    const int opChkW = S(148);
    const int opValW = S(42);
    const int fadeChkW = S(124);
    const int sliderW = (std::max)(S(120), rInnerW - opChkW - opValW - fadeChkW - gap * 3);
    MoveWindow(m_chkOpacity, rPad, oy + S(1), opChkW, chkH, TRUE);
    MoveWindow(m_sliderOpacity, rPad + opChkW + gap, oy, sliderW, rowH, TRUE);
    MoveWindow(m_lblOpacityVal, rPad + opChkW + gap + sliderW + gap, oy + S(1), opValW, chkH, TRUE);
    MoveWindow(m_chkSmoothFade, rPad + rInnerW - fadeChkW, oy + S(1), fadeChkW, chkH, TRUE);

    const int actY = sec3Y + sec3H + gap;
    const int testW = S(225);
    const int revW = S(168);
    const int saveW = S(175);
    MoveWindow(m_btnTestRule, rightX, actY, testW, actionH, TRUE);
    MoveWindow(m_btnRevertLast, rightX + testW + gap, actY, revW, actionH, TRUE);
    MoveWindow(m_btnSave, rightX + rightW - saveW, actY, saveW, actionH, TRUE);
}

void AppWindow::ApplyLanguage() {
    const bool elevated = WindowInspector::IsCurrentProcessElevated();
    SetWindowTextW(m_hwnd, elevated ? Tr(StrKey::AppTitleAdmin) : Tr(StrKey::AppTitle));
    m_tray.UpdateTooltip(Tr(StrKey::TrayTooltip));

    SetWindowTextW(m_lblProfile, Tr(StrKey::LblProfile));
    SetWindowTextW(m_btnNewProfile, Tr(StrKey::BtnNewProfile));
    SetWindowTextW(m_btnDelProfile, Tr(StrKey::BtnDelProfile));
    SetWindowTextW(m_btnExportProfile, Tr(StrKey::BtnExportProfile));
    SetWindowTextW(m_btnImportProfile, Tr(StrKey::BtnImportProfile));
    UpdateMonitorToggleButton();
    SetWindowTextW(m_btnApplyAll, Tr(StrKey::BtnApplyAll));
    SetWindowTextW(m_btnOpenWindows, Tr(StrKey::BtnOpenWindows));
    SetWindowTextW(m_btnLogs, Tr(StrKey::BtnLogs));
    SetWindowTextW(m_btnLang, Tr(StrKey::LangToggleBtn));
    SetWindowTextW(m_btnAdmin, elevated ? Tr(StrKey::BtnAdminActive) : Tr(StrKey::BtnAdmin));

    SetWindowTextW(m_grpRules, Tr(StrKey::GrpRules));
    SetWindowTextW(m_btnAddRule, Tr(StrKey::BtnAddRule));
    SetWindowTextW(m_btnDupRule, Tr(StrKey::BtnDupRule));
    SetWindowTextW(m_btnDelRule, Tr(StrKey::BtnDelRule));
    SetWindowTextW(m_btnMoveUp, Tr(StrKey::BtnMoveUp));
    SetWindowTextW(m_btnMoveDown, Tr(StrKey::BtnMoveDown));
    SetWindowTextW(m_lblHotkeyHint, Tr(StrKey::LblHotkeyHint));

    const StrKey colKeys[3] = {StrKey::ColRule, StrKey::ColTarget, StrKey::ColModeAction};
    for (int i = 0; i < 3; ++i) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT;
        col.pszText = const_cast<wchar_t*>(Tr(colKeys[i]));
        ListView_SetColumn(m_listRules, i, &col);
    }

    SetWindowTextW(m_grpMatch, Tr(StrKey::GrpMatch));
    SetWindowTextW(m_btnPickFromOpen, Tr(StrKey::BtnPickOpen));
    SetWindowTextW(m_lblPickedInfo, Tr(StrKey::PickedInfoDefault));
    SetWindowTextW(m_lblRuleName, Tr(StrKey::LblRuleName));
    SetWindowTextW(m_chkEnabled, Tr(StrKey::ChkEnabled));
    SetWindowTextW(m_chkProcess, Tr(StrKey::ChkProcess));
    SetWindowTextW(m_chkClass, Tr(StrKey::ChkClass));
    SetWindowTextW(m_lblTitle, Tr(StrKey::LblTitle));
    SetWindowTextW(m_chkExcludePopups, Tr(StrKey::ChkExcludePopups));
    SetWindowTextW(m_lblMinSrc, Tr(StrKey::LblMinSrc));

    SetWindowTextW(m_grpGeom, Tr(StrKey::GrpGeom));
    SetWindowTextW(m_lblWindowAction, Tr(StrKey::LblWindowAction));
    SetWindowTextW(m_lblActionDelay, Tr(StrKey::LblDelayMs));
    SetWindowTextW(m_lblGeomMode, Tr(StrKey::LblGeomMode));
    SetWindowTextW(m_lblMonitor, Tr(StrKey::LblMonitor));
    SetWindowTextW(m_btnPresetLeft, Tr(StrKey::PresetLeft));
    SetWindowTextW(m_btnPresetRight, Tr(StrKey::PresetRight));
    SetWindowTextW(m_btnPresetTop, Tr(StrKey::PresetTop));
    SetWindowTextW(m_btnPresetBottom, Tr(StrKey::PresetBottom));
    SetWindowTextW(m_btnPresetFull, Tr(StrKey::PresetFull));
    SetWindowTextW(m_btnPresetTL, Tr(StrKey::PresetTL));
    SetWindowTextW(m_btnPresetTR, Tr(StrKey::PresetTR));
    SetWindowTextW(m_btnPresetBL, Tr(StrKey::PresetBL));
    SetWindowTextW(m_btnPresetBR, Tr(StrKey::PresetBR));
    SetWindowTextW(m_btnPresetCenter, Tr(StrKey::PresetCenter));

    SetWindowTextW(m_grpOpts, Tr(StrKey::GrpOpts));
    SetWindowTextW(m_chkResizable, Tr(StrKey::ChkResizable));
    SetWindowTextW(m_chkBorderless, Tr(StrKey::ChkBorderless));
    SetWindowTextW(m_chkTopMost, Tr(StrKey::ChkTopMost));
    SetWindowTextW(m_chkDwmMargin, Tr(StrKey::ChkDwmMargin));
    SetWindowTextW(m_chkAggressive, Tr(StrKey::ChkAggressive));
    SetWindowTextW(m_chkHookDll, Tr(StrKey::ChkHookDll));
    SetWindowTextW(m_chkHitTest, Tr(StrKey::ChkHitTest));
    SetWindowTextW(m_chkLockSize, Tr(StrKey::ChkLockSize));
    SetWindowTextW(m_chkScaleChildren, Tr(StrKey::ChkScaleChildren));
    SetWindowTextW(m_chkCustomMinMax, Tr(StrKey::ChkCustomMinMax));
    SetWindowTextW(m_chkOpacity, Tr(StrKey::ChkOpacity));
    SetWindowTextW(m_chkSmoothFade, Tr(StrKey::ChkSmoothFade));

    SetWindowTextW(m_btnTestRule, Tr(StrKey::BtnTestRule));
    SetWindowTextW(m_btnRevertLast, Tr(StrKey::BtnRevertLast));
    SetWindowTextW(m_btnSave, Tr(StrKey::BtnSave));

    const int curRule = m_selectedRuleIndex;
    RepopulateTranslatedCombos();
    RefreshMonitorList();
    RefreshRulesList(curRule);
    m_picker.RefreshLanguage();
    m_preview.RefreshLanguage();
    m_openWinsDlg.RefreshLanguage();
    m_logDlg.RefreshLanguage();
    UpdateStatusBar();
}

void AppWindow::RefreshMonitorList() {
    m_monitors = MonitorManager::EnumerateMonitors();
    const int prevSel = static_cast<int>(SendMessageW(m_comboMonitor, CB_GETCURSEL, 0, 0));
    m_populatingUi = true;
    SendMessageW(m_comboMonitor, CB_RESETCONTENT, 0, 0);
    SendMessageW(m_comboMonitor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::MonWindowCurrent)));
    SendMessageW(m_comboMonitor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Tr(StrKey::MonPrimary)));
    for (size_t i = 0; i < m_monitors.size(); ++i) {
        wchar_t buf[96]{};
        swprintf_s(buf, Tr(StrKey::MonIndexFmt), i + 1,
                   m_monitors[i].monitorRect.right - m_monitors[i].monitorRect.left,
                   m_monitors[i].monitorRect.bottom - m_monitors[i].monitorRect.top,
                   m_monitors[i].isPrimary ? Tr(StrKey::MonPrimarySuffix) : L"");
        SendMessageW(m_comboMonitor, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(buf));
    }
    if (prevSel >= 0) {
        SendMessageW(m_comboMonitor, CB_SETCURSEL, static_cast<WPARAM>(prevSel), 0);
    }
    m_populatingUi = false;
}

void AppWindow::RefreshProfileCombo() {
    m_populatingUi = true;
    SendMessageW(m_comboProfile, CB_RESETCONTENT, 0, 0);
    for (const auto& p : m_config.profiles) {
        SendMessageW(m_comboProfile, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p.name.c_str()));
    }
    if (!m_config.profiles.empty()) {
        m_config.activeProfileIndex = std::clamp(
            m_config.activeProfileIndex, 0, static_cast<int>(m_config.profiles.size()) - 1);
        SendMessageW(m_comboProfile, CB_SETCURSEL, static_cast<WPARAM>(m_config.activeProfileIndex), 0);
    }
    EnableWindow(m_btnDelProfile, m_config.profiles.size() > 1);
    m_populatingUi = false;
}

Profile* AppWindow::GetActiveProfile() {
    if (m_config.profiles.empty()) return nullptr;
    int idx = std::clamp(m_config.activeProfileIndex, 0,
                         static_cast<int>(m_config.profiles.size()) - 1);
    return &m_config.profiles[static_cast<size_t>(idx)];
}

WindowRule* AppWindow::GetSelectedRule() {
    Profile* prof = GetActiveProfile();
    if (!prof || m_selectedRuleIndex < 0 ||
        m_selectedRuleIndex >= static_cast<int>(prof->rules.size())) {
        return nullptr;
    }
    return &prof->rules[static_cast<size_t>(m_selectedRuleIndex)];
}

void AppWindow::RefreshRulesList(int selectIdx) {
    m_populatingUi = true;
    ListView_DeleteAllItems(m_listRules);
    Profile* prof = GetActiveProfile();
    if (prof) {
        for (size_t i = 0; i < prof->rules.size(); ++i) {
            const auto& r = prof->rules[i];
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = static_cast<int>(i);
            item.pszText = const_cast<wchar_t*>(r.name.c_str());
            ListView_InsertItem(m_listRules, &item);

            std::wstring target = r.matchProcess && !r.processName.empty()
                                      ? r.processName
                                      : (r.matchClass ? r.className : r.titlePattern);
            ListView_SetItemText(m_listRules, static_cast<int>(i), 1, const_cast<wchar_t*>(target.c_str()));
            std::wstring summary = FormatRuleSummary(r);
            ListView_SetItemText(m_listRules, static_cast<int>(i), 2, const_cast<wchar_t*>(summary.c_str()));
            ListView_SetCheckState(m_listRules, static_cast<int>(i), r.enabled ? TRUE : FALSE);
        }

        if (!prof->rules.empty()) {
            int clamped = std::clamp(selectIdx, 0, static_cast<int>(prof->rules.size()) - 1);
            m_selectedRuleIndex = clamped;
            ListView_SetItemState(m_listRules, clamped, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        } else {
            m_selectedRuleIndex = -1;
        }
    } else {
        m_selectedRuleIndex = -1;
    }
    m_populatingUi = false;

    LoadRuleIntoEditor(m_selectedRuleIndex);
    UpdateStatusBar();
}

void AppWindow::SetEditorEnabled(bool enabled) {
    const int ids[] = {
        IDC_EDIT_RULE_NAME, IDC_CHK_ENABLED, IDC_CHK_PROCESS, IDC_EDIT_PROCESS,
        IDC_CHK_CLASS, IDC_EDIT_CLASS, IDC_COMBO_TITLE_MODE, IDC_EDIT_TITLE,
        IDC_CHK_EXCLUDE_POPUPS, IDC_EDIT_MIN_SRC_W, IDC_EDIT_MIN_SRC_H,
        IDC_COMBO_WINDOW_ACTION, IDC_EDIT_ACTION_DELAY,
        IDC_COMBO_GEOM_MODE, IDC_COMBO_MONITOR, IDC_EDIT_X, IDC_EDIT_Y, IDC_EDIT_W, IDC_EDIT_H,
        IDC_BTN_PRESET_LEFT, IDC_BTN_PRESET_RIGHT, IDC_BTN_PRESET_TOP, IDC_BTN_PRESET_BOTTOM,
        IDC_BTN_PRESET_FULL, IDC_BTN_PRESET_TL, IDC_BTN_PRESET_TR, IDC_BTN_PRESET_BL,
        IDC_BTN_PRESET_BR, IDC_BTN_PRESET_CENTER,
        IDC_CHK_RESIZABLE, IDC_CHK_BORDERLESS, IDC_CHK_TOPMOST, IDC_CHK_DWM_MARGIN,
        IDC_CHK_AGGRESSIVE, IDC_CHK_HOOK_DLL, IDC_CHK_HITTEST, IDC_CHK_LOCK_SIZE,
        IDC_CHK_SCALE_CHILDREN, IDC_CHK_CUSTOM_MINMAX,
        IDC_EDIT_MIN_W, IDC_EDIT_MIN_H, IDC_EDIT_MAX_W, IDC_EDIT_MAX_H,
        IDC_CHK_OPACITY, IDC_SLIDER_OPACITY, IDC_CHK_SMOOTH_FADE,
        IDC_BTN_TEST_RULE, IDC_BTN_DUP_RULE, IDC_BTN_DEL_RULE, IDC_BTN_MOVE_UP, IDC_BTN_MOVE_DOWN
    };
    for (int id : ids) {
        HWND h = GetDlgItem(m_hwnd, id);
        if (h) EnableWindow(h, enabled ? TRUE : FALSE);
    }
}

void AppWindow::LoadRuleIntoEditor(int idx) {
    m_selectedRuleIndex = idx;
    WindowRule* r = GetSelectedRule();
    if (!r) {
        SetEditorEnabled(false);
        m_preview.SetRulePreview(WindowRule{}, m_monitors);
        return;
    }

    SetEditorEnabled(true);
    m_populatingUi = true;

    SetWindowTextW(m_editRuleName, r->name.c_str());
    SendMessageW(m_chkEnabled, BM_SETCHECK, r->enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkProcess, BM_SETCHECK, r->matchProcess ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(m_editProcess, r->processName.c_str());
    SendMessageW(m_chkClass, BM_SETCHECK, r->matchClass ? BST_CHECKED : BST_UNCHECKED, 0);
    SetWindowTextW(m_editClass, r->className.c_str());
    SendMessageW(m_comboTitleMode, CB_SETCURSEL, static_cast<WPARAM>(r->titleMode), 0);
    SetWindowTextW(m_editTitle, r->titlePattern.c_str());
    SendMessageW(m_chkExcludePopups, BM_SETCHECK, r->excludePopupsAndDialogs ? BST_CHECKED : BST_UNCHECKED, 0);
    SetEditInt(m_editMinSrcW, r->minSourceWidth);
    SetEditInt(m_editMinSrcH, r->minSourceHeight);

    SendMessageW(m_comboWindowAction, CB_SETCURSEL, static_cast<WPARAM>(r->action), 0);
    SetEditInt(m_editActionDelay, r->actionDelayMs);

    SendMessageW(m_comboGeomMode, CB_SETCURSEL, static_cast<WPARAM>(r->geometryMode), 0);
    int monSel = 0;
    if (r->monitorMode == MonitorTargetMode::Primary) monSel = 1;
    else if (r->monitorMode == MonitorTargetMode::Index) {
        monSel = 2 + std::clamp(r->monitorIndex, 0, (std::max)(0, static_cast<int>(m_monitors.size()) - 1));
    }
    SendMessageW(m_comboMonitor, CB_SETCURSEL, static_cast<WPARAM>(monSel), 0);

    if (r->geometryMode == GeometryMode::Percent) {
        SetEditDouble(m_editX, r->xPct);
        SetEditDouble(m_editY, r->yPct);
        SetEditDouble(m_editW, r->widthPct);
        SetEditDouble(m_editH, r->heightPct);
    } else {
        SetEditInt(m_editX, r->x);
        SetEditInt(m_editY, r->y);
        SetEditInt(m_editW, r->width);
        SetEditInt(m_editH, r->height);
    }

    SendMessageW(m_chkResizable, BM_SETCHECK, r->makeResizable ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkBorderless, BM_SETCHECK, r->borderless ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkTopMost, BM_SETCHECK, r->alwaysOnTop ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkDwmMargin, BM_SETCHECK, r->compensateDwmMargin ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkAggressive, BM_SETCHECK, r->aggressiveForce ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkHookDll, BM_SETCHECK, r->useHookDll ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkHitTest, BM_SETCHECK, r->unlockHitTest ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkLockSize, BM_SETCHECK, r->lockTargetSize ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkScaleChildren, BM_SETCHECK, r->scaleChildControls ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkCustomMinMax, BM_SETCHECK, r->customMinMax ? BST_CHECKED : BST_UNCHECKED, 0);
    SetEditInt(m_editMinW, r->customMinW);
    SetEditInt(m_editMinH, r->customMinH);
    SetEditInt(m_editMaxW, r->customMaxW);
    SetEditInt(m_editMaxH, r->customMaxH);

    const int op = std::clamp(r->opacityPercent, 10, 100);
    SendMessageW(m_chkOpacity, BM_SETCHECK, r->enableOpacity ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_sliderOpacity, TBM_SETPOS, TRUE, op);
    SetWindowTextW(m_lblOpacityVal, (std::to_wstring(op) + L"%").c_str());
    SendMessageW(m_chkSmoothFade, BM_SETCHECK, r->smoothOpacityFade ? BST_CHECKED : BST_UNCHECKED, 0);

    m_populatingUi = false;
    UpdateGeometryLabels();
    UpdatePreview();
}

void AppWindow::UpdateGeometryLabels() {
    WindowRule* r = GetSelectedRule();
    if (!r) return;

    const bool isPct = (r->geometryMode == GeometryMode::Percent);
    SetWindowTextW(m_lblX, isPct ? Tr(StrKey::LblXPct) : Tr(StrKey::LblXPx));
    SetWindowTextW(m_lblY, isPct ? Tr(StrKey::LblYPct) : Tr(StrKey::LblYPx));
    SetWindowTextW(m_lblW, isPct ? Tr(StrKey::LblWPct) : Tr(StrKey::LblWPx));
    SetWindowTextW(m_lblH, isPct ? Tr(StrKey::LblHPct) : Tr(StrKey::LblHPx));

    const bool normalAction = (r->action == WindowAction::Normal);
    EnableWindow(m_editActionDelay, !normalAction);

    const bool hasPos = normalAction && (r->geometryMode == GeometryMode::Pixels ||
                                         r->geometryMode == GeometryMode::Percent ||
                                         r->geometryMode == GeometryMode::PositionOnlyPixels);
    const bool hasSize = normalAction && (r->geometryMode == GeometryMode::Pixels ||
                                          r->geometryMode == GeometryMode::Percent ||
                                          r->geometryMode == GeometryMode::SizeOnlyPixels);
    EnableWindow(m_comboGeomMode, normalAction);
    EnableWindow(m_comboMonitor, normalAction);
    EnableWindow(m_editX, hasPos);
    EnableWindow(m_editY, hasPos);
    EnableWindow(m_editW, hasSize);
    EnableWindow(m_editH, hasSize);

    const int presetIds[] = {
        IDC_BTN_PRESET_LEFT, IDC_BTN_PRESET_RIGHT, IDC_BTN_PRESET_TOP, IDC_BTN_PRESET_BOTTOM,
        IDC_BTN_PRESET_FULL, IDC_BTN_PRESET_TL, IDC_BTN_PRESET_TR, IDC_BTN_PRESET_BL,
        IDC_BTN_PRESET_BR, IDC_BTN_PRESET_CENTER
    };
    for (int id : presetIds) {
        if (HWND h = GetDlgItem(m_hwnd, id)) EnableWindow(h, normalAction);
    }

    const int normalOnlyOpts[] = {
        IDC_CHK_RESIZABLE, IDC_CHK_BORDERLESS, IDC_CHK_TOPMOST, IDC_CHK_DWM_MARGIN,
        IDC_CHK_AGGRESSIVE, IDC_CHK_HOOK_DLL, IDC_CHK_HITTEST, IDC_CHK_LOCK_SIZE,
        IDC_CHK_SCALE_CHILDREN, IDC_CHK_CUSTOM_MINMAX, IDC_CHK_OPACITY
    };
    for (int id : normalOnlyOpts) {
        if (HWND h = GetDlgItem(m_hwnd, id)) EnableWindow(h, normalAction);
    }

    const bool customMM = normalAction && (SendMessageW(m_chkCustomMinMax, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(m_editMinW, customMM);
    EnableWindow(m_editMinH, customMM);
    EnableWindow(m_editMaxW, customMM);
    EnableWindow(m_editMaxH, customMM);

    const bool opEnabled = normalAction && (SendMessageW(m_chkOpacity, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(m_sliderOpacity, opEnabled);
    EnableWindow(m_chkSmoothFade, opEnabled);

    EnableWindow(m_editProcess, SendMessageW(m_chkProcess, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(m_editClass, SendMessageW(m_chkClass, BM_GETCHECK, 0, 0) == BST_CHECKED);
    EnableWindow(m_editTitle, SendMessageW(m_comboTitleMode, CB_GETCURSEL, 0, 0) != 0);
}

void AppWindow::ReadEditorIntoCurrentRule() {
    if (m_populatingUi) return;
    WindowRule* r = GetSelectedRule();
    if (!r) return;

    r->name = GetEditText(m_editRuleName);
    r->enabled = (SendMessageW(m_chkEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->matchProcess = (SendMessageW(m_chkProcess, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->processName = TrimWide(GetEditText(m_editProcess));
    r->matchClass = (SendMessageW(m_chkClass, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->className = TrimWide(GetEditText(m_editClass));
    int tm = static_cast<int>(SendMessageW(m_comboTitleMode, CB_GETCURSEL, 0, 0));
    r->titleMode = static_cast<TitleMatchMode>(std::clamp(tm, 0, 4));
    r->titlePattern = GetEditText(m_editTitle);
    r->excludePopupsAndDialogs = (SendMessageW(m_chkExcludePopups, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->minSourceWidth = (std::max)(0, GetEditInt(m_editMinSrcW, 120));
    r->minSourceHeight = (std::max)(0, GetEditInt(m_editMinSrcH, 80));

    int act = static_cast<int>(SendMessageW(m_comboWindowAction, CB_GETCURSEL, 0, 0));
    r->action = static_cast<WindowAction>(std::clamp(act, 0, 2));
    r->actionDelayMs = std::clamp(GetEditInt(m_editActionDelay, 0), 0, 60000);

    int gm = static_cast<int>(SendMessageW(m_comboGeomMode, CB_GETCURSEL, 0, 0));
    r->geometryMode = static_cast<GeometryMode>(std::clamp(gm, 0, 4));

    int ms = static_cast<int>(SendMessageW(m_comboMonitor, CB_GETCURSEL, 0, 0));
    if (ms <= 0) r->monitorMode = MonitorTargetMode::WindowCurrent;
    else if (ms == 1) r->monitorMode = MonitorTargetMode::Primary;
    else {
        r->monitorMode = MonitorTargetMode::Index;
        r->monitorIndex = ms - 2;
    }

    if (r->geometryMode == GeometryMode::Percent) {
        r->xPct = GetEditDouble(m_editX, r->xPct);
        r->yPct = GetEditDouble(m_editY, r->yPct);
        r->widthPct = GetEditDouble(m_editW, r->widthPct);
        r->heightPct = GetEditDouble(m_editH, r->heightPct);
    } else {
        r->x = GetEditInt(m_editX, r->x);
        r->y = GetEditInt(m_editY, r->y);
        r->width = GetEditInt(m_editW, r->width);
        r->height = GetEditInt(m_editH, r->height);
    }

    r->makeResizable = (SendMessageW(m_chkResizable, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->borderless = (SendMessageW(m_chkBorderless, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->alwaysOnTop = (SendMessageW(m_chkTopMost, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->compensateDwmMargin = (SendMessageW(m_chkDwmMargin, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->aggressiveForce = (SendMessageW(m_chkAggressive, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->useHookDll = (SendMessageW(m_chkHookDll, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->unlockHitTest = (SendMessageW(m_chkHitTest, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->lockTargetSize = (SendMessageW(m_chkLockSize, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->scaleChildControls = (SendMessageW(m_chkScaleChildren, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->customMinMax = (SendMessageW(m_chkCustomMinMax, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->customMinW = GetEditInt(m_editMinW, 320);
    r->customMinH = GetEditInt(m_editMinH, 240);
    r->customMaxW = GetEditInt(m_editMaxW, 0);
    r->customMaxH = GetEditInt(m_editMaxH, 0);

    r->enableOpacity = (SendMessageW(m_chkOpacity, BM_GETCHECK, 0, 0) == BST_CHECKED);
    r->opacityPercent = std::clamp(static_cast<int>(SendMessageW(m_sliderOpacity, TBM_GETPOS, 0, 0)), 10, 100);
    r->smoothOpacityFade = (SendMessageW(m_chkSmoothFade, BM_GETCHECK, 0, 0) == BST_CHECKED);
    SetWindowTextW(m_lblOpacityVal, (std::to_wstring(r->opacityPercent) + L"%").c_str());

    m_populatingUi = true;
    ListView_SetItemText(m_listRules, m_selectedRuleIndex, 0, const_cast<wchar_t*>(r->name.c_str()));
    std::wstring target = r->matchProcess && !r->processName.empty()
                              ? r->processName
                              : (r->matchClass ? r->className : r->titlePattern);
    ListView_SetItemText(m_listRules, m_selectedRuleIndex, 1, const_cast<wchar_t*>(target.c_str()));
    std::wstring summary = FormatRuleSummary(*r);
    ListView_SetItemText(m_listRules, m_selectedRuleIndex, 2, const_cast<wchar_t*>(summary.c_str()));
    ListView_SetCheckState(m_listRules, m_selectedRuleIndex, r->enabled ? TRUE : FALSE);
    m_populatingUi = false;

    UpdatePreview();
}

void AppWindow::ApplyPreset(double xp, double yp, double wp, double hp) {
    WindowRule* r = GetSelectedRule();
    if (!r) return;

    r->action = WindowAction::Normal;
    r->geometryMode = GeometryMode::Percent;
    r->xPct = xp;
    r->yPct = yp;
    r->widthPct = wp;
    r->heightPct = hp;
    LoadRuleIntoEditor(m_selectedRuleIndex);
}

void AppWindow::UpdatePreview() {
    WindowRule* r = GetSelectedRule();
    m_preview.SetRulePreview(r ? *r : WindowRule{}, m_monitors);
}

void AppWindow::PopulateRuleFromWindow(const WindowInfo& info, bool createNewRule) {
    Profile* prof = GetActiveProfile();
    if (!prof) return;

    WindowRule* r = createNewRule ? nullptr : GetSelectedRule();
    if (!r) {
        WindowRule nr;
        nr.id = ConfigManager::GenerateRuleId();
        nr.name = info.processName.empty() ? Tr(StrKey::CapturedRuleFallback) : info.processName;
        prof->rules.push_back(std::move(nr));
        m_selectedRuleIndex = static_cast<int>(prof->rules.size()) - 1;
        r = &prof->rules.back();
    }

    r->enabled = true;
    r->matchProcess = !info.processName.empty();
    r->processName = info.processName;
    r->matchClass = !info.className.empty() && info.isDialogClass;
    r->className = info.className;
    if (r->name == Tr(StrKey::DefaultNewRuleName) || r->name.empty()) {
        r->name = info.processName.empty() ? info.className : info.processName;
    }
    if (info.isDialogClass || info.isOwnedPopup) {
        r->excludePopupsAndDialogs = false;
    }

    const int curW = info.visualRect.right - info.visualRect.left;
    const int curH = info.visualRect.bottom - info.visualRect.top;
    if (curW > 0 && curH > 0) {
        r->width = (std::max)(curW, 100);
        r->height = (std::max)(curH, 80);
    }

    wchar_t summary[512]{};
    swprintf_s(summary, L"%s | %s | %dx%d | %s%s",
               info.processName.c_str(), info.className.c_str(),
               curW, curH,
               info.is32Bit ? L"x86" : L"x64",
               info.isElevated ? L" | Admin!" : L"");
    SetWindowTextW(m_lblPickedInfo, summary);

    RefreshRulesList(m_selectedRuleIndex);
}

void AppWindow::SaveConfig(bool reapplyEngine) {
    ReadEditorIntoCurrentRule();
    m_config.language = I18n::GetLanguageCode();
    std::wstring err;
    if (ConfigManager::Save(m_configPath, m_config, &err)) {
        if (reapplyEngine) {
            m_engine.UpdateConfig(m_config);
            m_hotkeys.RegisterFromConfig(m_hwnd, m_config);
        }
        UpdateMonitorToggleButton();
        UpdateStatusBar();
        Logger::Instance().Info(Tr(StrKey::ConfigSavedLog));
    } else {
        MessageBoxW(m_hwnd, err.c_str(), Tr(StrKey::SaveErrorTitle), MB_ICONERROR);
    }
}

void AppWindow::ExportActiveProfileDialog() {
    ReadEditorIntoCurrentRule();
    Profile* prof = GetActiveProfile();
    if (!prof) return;

    wchar_t fileBuf[MAX_PATH]{};
    const std::wstring defName = SanitizeFileName(prof->name) + L".json";
    wcsncpy_s(fileBuf, defName.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"FlexWin Profile JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"json";
    ofn.lpstrTitle = Tr(StrKey::ExportDialogTitle);
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&ofn)) return;

    std::wstring err;
    if (ConfigManager::ExportProfile(fileBuf, *prof, &err)) {
        Logger::Instance().Success(std::wstring(Tr(StrKey::ExportSuccessMsg)) + L" -> " + fileBuf);
        MessageBoxW(m_hwnd, Tr(StrKey::ExportSuccessMsg), Tr(StrKey::ExportDialogTitle), MB_ICONINFORMATION);
    } else {
        MessageBoxW(m_hwnd, err.c_str(), Tr(StrKey::ExportErrorTitle), MB_ICONERROR);
    }
}

void AppWindow::ImportProfilesDialog() {
    ReadEditorIntoCurrentRule();

    wchar_t fileBuf[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwnd;
    ofn.lpstrFilter = L"FlexWin JSON (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = Tr(StrKey::ImportDialogTitle);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn)) return;

    std::vector<Profile> imported;
    std::wstring err;
    if (!ConfigManager::ImportProfiles(fileBuf, imported, &err)) {
        MessageBoxW(m_hwnd, err.c_str(), Tr(StrKey::ImportErrorTitle), MB_ICONERROR);
        return;
    }

    auto nameExists = [&](const std::wstring& candidate) {
        for (const auto& existing : m_config.profiles) {
            if (EqualsIgnoreCase(existing.name, candidate)) return true;
        }
        return false;
    };

    const int firstNewIdx = static_cast<int>(m_config.profiles.size());
    for (auto& p : imported) {
        std::wstring uniqueName = p.name.empty() ? Tr(StrKey::DefaultNewProfileName) : p.name;
        int suffix = 2;
        while (nameExists(uniqueName)) {
            uniqueName = p.name + L" (" + std::to_wstring(suffix++) + L")";
        }
        p.name = uniqueName;
        m_config.profiles.push_back(std::move(p));
    }
    m_config.activeProfileIndex = firstNewIdx;
    RefreshProfileCombo();
    RefreshRulesList(0);
    SaveConfig(true);

    wchar_t msg[128]{};
    swprintf_s(msg, Tr(StrKey::ImportSuccessFmt), imported.size());
    Logger::Instance().Success(msg);
    MessageBoxW(m_hwnd, msg, Tr(StrKey::ImportDialogTitle), MB_ICONINFORMATION);
}

void AppWindow::UpdateMonitorToggleButton() {
    SetWindowTextW(m_btnToggleMonitor,
                   m_config.monitoringEnabled ? Tr(StrKey::BtnMonitorOn) : Tr(StrKey::BtnMonitorOff));
}

void AppWindow::UpdateStatusBar() {
    if (!m_statusBar) return;
    Profile* prof = GetActiveProfile();
    size_t enabledCount = 0;
    size_t totalCount = prof ? prof->rules.size() : 0;
    if (prof) {
        for (const auto& r : prof->rules) {
            if (r.enabled) ++enabledCount;
        }
    }

    wchar_t p0[128]{};
    swprintf_s(p0, Tr(StrKey::SbProfileFmt),
               prof ? prof->name.c_str() : L"-", enabledCount, totalCount);
    SendMessageW(m_statusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(p0));

    wchar_t p1[160]{};
    const auto& hb = m_engine.GetHookBridge();
    swprintf_s(p1, Tr(StrKey::SbHookFmt),
               hb.IsHook64Ready() ? Tr(StrKey::SbHookReady) : Tr(StrKey::SbHookMissing),
               hb.IsHook32Available() ? Tr(StrKey::SbHookReady) : Tr(StrKey::SbHookMissing),
               hb.ActiveHookCount());
    SendMessageW(m_statusBar, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(p1));

    const LogEntry latest = Logger::Instance().Latest();
    std::wstring p2 = latest.message.empty()
                          ? std::wstring(Tr(StrKey::SbReady))
                          : (latest.timestamp + L" " + latest.message);
    SendMessageW(m_statusBar, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(p2.c_str()));
}

void AppWindow::ShowOrRestoreWindow() {
    ShowWindow(m_hwnd, SW_SHOW);
    if (IsIconic(m_hwnd)) ShowWindow(m_hwnd, SW_RESTORE);
    SetForegroundWindow(m_hwnd);
}

void AppWindow::HandleTrayCommand(UINT cmdId) {
    if (cmdId >= IDM_TRAY_PROFILE_BASE &&
        cmdId < IDM_TRAY_PROFILE_BASE + static_cast<UINT>(m_config.profiles.size())) {
        m_config.activeProfileIndex = static_cast<int>(cmdId - IDM_TRAY_PROFILE_BASE);
        RefreshProfileCombo();
        RefreshRulesList(0);
        SaveConfig(true);
        if (m_config.monitoringEnabled) {
            m_engine.ApplyActiveProfileToAllWindows();
        }
        return;
    }

    switch (cmdId) {
    case IDM_TRAY_SHOW:
        ShowOrRestoreWindow();
        break;
    case IDM_TRAY_APPLY_ALL: {
        int count = m_engine.ApplyActiveProfileToAllWindows();
        wchar_t buf[128]{};
        swprintf_s(buf, Tr(StrKey::AppliedAllFmt), count);
        Logger::Instance().Success(buf);
        UpdateStatusBar();
        break;
    }
    case IDM_TRAY_TOGGLE_MONITOR:
        m_config.monitoringEnabled = !m_config.monitoringEnabled;
        SaveConfig(true);
        break;
    case IDM_TRAY_EXPORT_PROFILE:
        ExportActiveProfileDialog();
        break;
    case IDM_TRAY_IMPORT_PROFILE:
        ImportProfilesDialog();
        break;
    case IDM_TRAY_LANG_TR:
        I18n::SetLanguage(Language::Tr);
        m_config.language = L"tr";
        ApplyLanguage();
        SaveConfig(false);
        break;
    case IDM_TRAY_LANG_EN:
        I18n::SetLanguage(Language::En);
        m_config.language = L"en";
        ApplyLanguage();
        SaveConfig(false);
        break;
    case IDM_TRAY_STARTUP:
        m_config.runAtStartup = !m_config.runAtStartup;
        ConfigManager::SetRunAtStartup(m_config.runAtStartup);
        SaveConfig(false);
        break;
    case IDM_TRAY_START_MIN:
        m_config.startMinimized = !m_config.startMinimized;
        SaveConfig(false);
        break;
    case IDM_TRAY_CLOSE_TO_TRAY:
        m_config.minimizeToTrayOnClose = !m_config.minimizeToTrayOnClose;
        SaveConfig(false);
        break;
    case IDM_TRAY_TOGGLE_HOOKS:
        m_config.enableHookEngine = !m_config.enableHookEngine;
        SaveConfig(true);
        break;
    case IDM_TRAY_RUN_ADMIN:
        if (WindowInspector::RelaunchAsAdmin()) {
            SaveConfig(false);
            Shutdown();
            DestroyWindow(m_hwnd);
        }
        break;
    case IDM_TRAY_EXIT:
        SaveConfig(false);
        Shutdown();
        DestroyWindow(m_hwnd);
        break;
    }
}

void AppWindow::HandleHotkey(int hotkeyId) {
    RegisteredHotkeyInfo info;
    if (!m_hotkeys.Lookup(hotkeyId, info)) return;

    HWND fg = GetForegroundWindow();
    switch (info.kind) {
    case HotkeyActionKind::UnlockActive:
        if (fg) m_engine.QuickUnlockWindow(fg);
        break;
    case HotkeyActionKind::ToggleBorderlessActive:
        if (fg) m_engine.QuickToggleBorderless(fg);
        break;
    case HotkeyActionKind::ToggleTopMostActive:
        if (fg) m_engine.QuickToggleTopMost(fg);
        break;
    case HotkeyActionKind::ApplyAllRules:
        m_engine.ApplyActiveProfileToAllWindows();
        break;
    case HotkeyActionKind::ApplyRuleById:
        if (fg) {
            if (Profile* prof = GetActiveProfile()) {
                for (const auto& r : prof->rules) {
                    if (r.id == info.ruleId) {
                        m_engine.ApplyRuleToWindow(fg, r, true);
                        break;
                    }
                }
            }
        }
        break;
    }
    UpdateStatusBar();
}

LRESULT AppWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateControls();
        return 0;

    case WM_SIZE:
        LayoutControls(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = DpiScale(1040, m_dpi);
        mmi->ptMinTrackSize.y = DpiScale(720, m_dpi);
        return 0;
    }

    case WM_DPICHANGED: {
        m_dpi = HIWORD(wParam);
        CreateFontsForDpi(m_dpi);
        ApplyFontsToChildren();
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(m_hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_DISPLAYCHANGE:
        RefreshMonitorList();
        LoadRuleIntoEditor(m_selectedRuleIndex);
        return 0;

    case WM_APP_START_ANIM_TIMER:
        if (!m_animTimerRunning) {
            SetTimer(m_hwnd, kAnimTimerId, 30, nullptr);
            m_animTimerRunning = true;
        }
        return 0;

    case WM_TIMER:
        if (wParam == kMaintTimerId) {
            m_engine.PeriodicMaintenance();
            UpdateStatusBar();
        } else if (wParam == kAnimTimerId) {
            if (!m_engine.ProcessTimers()) {
                KillTimer(m_hwnd, kAnimTimerId);
                m_animTimerRunning = false;
            }
        }
        return 0;

    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == m_sliderOpacity && !m_populatingUi) {
            ReadEditorIntoCurrentRule();
        }
        return 0;

    case WM_APP_LOG_UPDATE:
        UpdateStatusBar();
        if (IsWindowVisible(m_hwnd)) {
            m_logDlg.Refresh();
        }
        return 0;

    case WM_HOTKEY:
        HandleHotkey(static_cast<int>(wParam));
        return 0;

    case WM_APP_TRAY: {
        const UINT ev = LOWORD(lParam);
        if (ev == WM_LBUTTONDBLCLK || ev == NIN_SELECT || ev == NIN_KEYSELECT) {
            ShowOrRestoreWindow();
        } else if (ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU) {
            m_tray.ShowContextMenu(m_hwnd, m_config, WindowInspector::IsCurrentProcessElevated());
        }
        return 0;
    }

    case WM_NOTIFY: {
        auto* nm = reinterpret_cast<NMHDR*>(lParam);
        if (nm->idFrom == IDC_LIST_RULES && nm->code == LVN_ITEMCHANGED && !m_populatingUi) {
            auto* nmlv = reinterpret_cast<NMLISTVIEW*>(lParam);
            if ((nmlv->uChanged & LVIF_STATE) &&
                ((nmlv->uNewState ^ nmlv->uOldState) & LVIS_STATEIMAGEMASK)) {
                Profile* prof = GetActiveProfile();
                if (prof && nmlv->iItem >= 0 && nmlv->iItem < static_cast<int>(prof->rules.size())) {
                    const bool checked = (ListView_GetCheckState(m_listRules, nmlv->iItem) != 0);
                    prof->rules[static_cast<size_t>(nmlv->iItem)].enabled = checked;
                    if (nmlv->iItem == m_selectedRuleIndex) {
                        m_populatingUi = true;
                        SendMessageW(m_chkEnabled, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
                        m_populatingUi = false;
                    }
                }
            }
            if ((nmlv->uChanged & LVIF_STATE) && (nmlv->uNewState & LVIS_SELECTED)) {
                if (nmlv->iItem != m_selectedRuleIndex) {
                    ReadEditorIntoCurrentRule();
                    LoadRuleIntoEditor(nmlv->iItem);
                }
            }
        }
        return 0;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);

        if (id >= IDM_TRAY_SHOW && id < IDM_TRAY_PROFILE_BASE + 256) {
            HandleTrayCommand(static_cast<UINT>(id));
            return 0;
        }

        if (id == IDC_COMBO_PROFILE && code == CBN_SELCHANGE && !m_populatingUi) {
            ReadEditorIntoCurrentRule();
            int sel = static_cast<int>(SendMessageW(m_comboProfile, CB_GETCURSEL, 0, 0));
            if (sel >= 0 && sel < static_cast<int>(m_config.profiles.size())) {
                m_config.activeProfileIndex = sel;
                RefreshRulesList(0);
                SaveConfig(true);
            }
            return 0;
        }
        if (id == IDC_BTN_NEW_PROFILE) {
            std::wstring name = Tr(StrKey::DefaultNewProfileName);
            if (ShowTextPrompt(m_hwnd, m_hInst, m_font, m_dpi,
                               Tr(StrKey::PromptNewProfileTitle), Tr(StrKey::PromptNewProfileLabel), name)) {
                ReadEditorIntoCurrentRule();
                Profile p;
                p.name = name;
                m_config.profiles.push_back(std::move(p));
                m_config.activeProfileIndex = static_cast<int>(m_config.profiles.size()) - 1;
                RefreshProfileCombo();
                RefreshRulesList(0);
                SaveConfig(true);
            }
            return 0;
        }
        if (id == IDC_BTN_DEL_PROFILE) {
            if (m_config.profiles.size() > 1) {
                m_config.profiles.erase(m_config.profiles.begin() + m_config.activeProfileIndex);
                m_config.activeProfileIndex = std::clamp(
                    m_config.activeProfileIndex, 0, static_cast<int>(m_config.profiles.size()) - 1);
                RefreshProfileCombo();
                RefreshRulesList(0);
                SaveConfig(true);
            }
            return 0;
        }
        if (id == IDC_BTN_EXPORT_PROFILE) {
            ExportActiveProfileDialog();
            return 0;
        }
        if (id == IDC_BTN_IMPORT_PROFILE) {
            ImportProfilesDialog();
            return 0;
        }
        if (id == IDC_BTN_LANG) {
            const Language next = (I18n::GetLanguage() == Language::Tr) ? Language::En : Language::Tr;
            I18n::SetLanguage(next);
            m_config.language = I18n::GetLanguageCode();
            ApplyLanguage();
            SaveConfig(false);
            return 0;
        }
        if (id == IDC_BTN_TOGGLE_MONITOR) {
            m_config.monitoringEnabled = !m_config.monitoringEnabled;
            SaveConfig(true);
            return 0;
        }
        if (id == IDC_BTN_APPLY_ALL) {
            SaveConfig(true);
            int count = m_engine.ApplyActiveProfileToAllWindows();
            wchar_t buf[128]{};
            swprintf_s(buf, Tr(StrKey::AppliedAllFmt), count);
            Logger::Instance().Success(buf);
            UpdateStatusBar();
            return 0;
        }
        if (id == IDC_BTN_OPEN_WINDOWS || id == IDC_BTN_PICK_OPEN) {
            m_openWinsDlg.SetOnCreateRule([this](const WindowInfo& wi) {
                PopulateRuleFromWindow(wi, true);
            });
            m_openWinsDlg.SetOnQuickUnlock([this](const WindowInfo& wi) {
                m_engine.QuickUnlockWindow(wi.hwnd);
                UpdateStatusBar();
            });
            m_openWinsDlg.SetOnQuickMinimize([this](const WindowInfo& wi) {
                m_engine.QuickMinimizeWindow(wi.hwnd);
                UpdateStatusBar();
            });
            m_openWinsDlg.SetOnQuickClose([this](const WindowInfo& wi) {
                m_engine.QuickCloseWindow(wi.hwnd);
                UpdateStatusBar();
            });
            m_openWinsDlg.Show(m_hwnd, m_hInst, m_font);
            return 0;
        }
        if (id == IDC_BTN_LOGS) {
            m_logDlg.Show(m_hwnd, m_hInst, m_font);
            return 0;
        }
        if (id == IDC_BTN_ADMIN) {
            if (WindowInspector::RelaunchAsAdmin()) {
                SaveConfig(false);
                Shutdown();
                DestroyWindow(m_hwnd);
            }
            return 0;
        }

        // Rule list buttons
        if (id == IDC_BTN_ADD_RULE) {
            ReadEditorIntoCurrentRule();
            if (Profile* prof = GetActiveProfile()) {
                WindowRule r;
                r.id = ConfigManager::GenerateRuleId();
                r.name = std::wstring(Tr(StrKey::DefaultNewRuleName)) + L" " +
                         std::to_wstring(prof->rules.size() + 1);
                prof->rules.push_back(std::move(r));
                RefreshRulesList(static_cast<int>(prof->rules.size()) - 1);
            }
            return 0;
        }
        if (id == IDC_BTN_DUP_RULE) {
            ReadEditorIntoCurrentRule();
            if (Profile* prof = GetActiveProfile(); prof && GetSelectedRule()) {
                WindowRule copy = *GetSelectedRule();
                copy.id = ConfigManager::GenerateRuleId();
                copy.name += Tr(StrKey::CopySuffix);
                prof->rules.insert(prof->rules.begin() + m_selectedRuleIndex + 1, std::move(copy));
                RefreshRulesList(m_selectedRuleIndex + 1);
            }
            return 0;
        }
        if (id == IDC_BTN_DEL_RULE) {
            if (Profile* prof = GetActiveProfile(); prof && GetSelectedRule()) {
                prof->rules.erase(prof->rules.begin() + m_selectedRuleIndex);
                RefreshRulesList(m_selectedRuleIndex);
                SaveConfig(true);
            }
            return 0;
        }
        if (id == IDC_BTN_MOVE_UP) {
            ReadEditorIntoCurrentRule();
            if (Profile* prof = GetActiveProfile(); prof && m_selectedRuleIndex > 0) {
                std::swap(prof->rules[static_cast<size_t>(m_selectedRuleIndex)],
                          prof->rules[static_cast<size_t>(m_selectedRuleIndex - 1)]);
                RefreshRulesList(m_selectedRuleIndex - 1);
            }
            return 0;
        }
        if (id == IDC_BTN_MOVE_DOWN) {
            ReadEditorIntoCurrentRule();
            if (Profile* prof = GetActiveProfile();
                prof && m_selectedRuleIndex >= 0 &&
                m_selectedRuleIndex + 1 < static_cast<int>(prof->rules.size())) {
                std::swap(prof->rules[static_cast<size_t>(m_selectedRuleIndex)],
                          prof->rules[static_cast<size_t>(m_selectedRuleIndex + 1)]);
                RefreshRulesList(m_selectedRuleIndex + 1);
            }
            return 0;
        }

        // Preset buttons
        if (id == IDC_BTN_PRESET_LEFT)   { ApplyPreset(0, 0, 50, 100); return 0; }
        if (id == IDC_BTN_PRESET_RIGHT)  { ApplyPreset(50, 0, 50, 100); return 0; }
        if (id == IDC_BTN_PRESET_TOP)    { ApplyPreset(0, 0, 100, 50); return 0; }
        if (id == IDC_BTN_PRESET_BOTTOM) { ApplyPreset(0, 50, 100, 50); return 0; }
        if (id == IDC_BTN_PRESET_FULL)   { ApplyPreset(0, 0, 100, 100); return 0; }
        if (id == IDC_BTN_PRESET_TL)     { ApplyPreset(0, 0, 50, 50); return 0; }
        if (id == IDC_BTN_PRESET_TR)     { ApplyPreset(50, 0, 50, 50); return 0; }
        if (id == IDC_BTN_PRESET_BL)     { ApplyPreset(0, 50, 50, 50); return 0; }
        if (id == IDC_BTN_PRESET_BR)     { ApplyPreset(50, 50, 50, 50); return 0; }
        if (id == IDC_BTN_PRESET_CENTER) { ApplyPreset(10, 10, 80, 80); return 0; }

        // Action buttons
        if (id == IDC_BTN_SAVE) {
            SaveConfig(true);
            return 0;
        }
        if (id == IDC_BTN_TEST_RULE) {
            SaveConfig(true);
            WindowRule* r = GetSelectedRule();
            if (!r) return 0;
            auto wins = WindowInspector::EnumerateTopLevelWindows(false);
            int hitCount = 0;
            for (const auto& wi : wins) {
                if (WindowInspector::IsSystemProtectedWindow(wi.hwnd, wi.className, wi.pid)) continue;
                if (WindowEngine::MatchesRule(*r, wi, true)) {
                    if (m_engine.ApplyRuleToWindow(wi.hwnd, *r, true)) {
                        m_lastTestedHwnd = wi.hwnd;
                        ++hitCount;
                    }
                }
            }
            if (hitCount == 0) {
                MessageBoxW(m_hwnd, Tr(StrKey::TestNoMatchBody), Tr(StrKey::TestNoMatchTitle), MB_ICONINFORMATION);
            }
            UpdateStatusBar();
            return 0;
        }
        if (id == IDC_BTN_REVERT_LAST) {
            if (m_lastTestedHwnd && IsWindow(m_lastTestedHwnd)) {
                m_engine.RevertWindow(m_lastTestedHwnd);
                UpdateStatusBar();
            } else {
                Logger::Instance().Warn(Tr(StrKey::RevertNotFound));
            }
            return 0;
        }

        if (id == IDC_COMBO_GEOM_MODE && code == CBN_SELCHANGE && !m_populatingUi) {
            if (WindowRule* r = GetSelectedRule()) {
                int gm = static_cast<int>(SendMessageW(m_comboGeomMode, CB_GETCURSEL, 0, 0));
                r->geometryMode = static_cast<GeometryMode>(std::clamp(gm, 0, 4));
                LoadRuleIntoEditor(m_selectedRuleIndex);
            }
            return 0;
        }

        if (!m_populatingUi &&
            ((code == EN_CHANGE && ((id >= IDC_EDIT_RULE_NAME && id <= IDC_EDIT_MAX_H) || id == IDC_EDIT_ACTION_DELAY)) ||
             (code == CBN_SELCHANGE && (id == IDC_COMBO_TITLE_MODE || id == IDC_COMBO_MONITOR || id == IDC_COMBO_WINDOW_ACTION)) ||
             (code == BN_CLICKED &&
              ((id >= IDC_CHK_ENABLED && id <= IDC_CHK_EXCLUDE_POPUPS) ||
               (id >= IDC_CHK_RESIZABLE && id <= IDC_CHK_SMOOTH_FADE))))) {
            ReadEditorIntoCurrentRule();
            UpdateGeometryLabels();
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        if (!m_exiting && m_config.minimizeToTrayOnClose) {
            SaveConfig(false);
            ShowWindow(m_hwnd, SW_HIDE);
            return 0;
        }
        SaveConfig(false);
        Shutdown();
        DestroyWindow(m_hwnd);
        return 0;

    case WM_ENDSESSION:
        if (wParam) {
            SaveConfig(false);
            Shutdown();
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    if (g_msgShowMainUi != 0 && msg == g_msgShowMainUi) {
        ShowOrRestoreWindow();
        return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

} // namespace flexwin
