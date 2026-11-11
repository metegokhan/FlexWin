#include "LogDialog.h"

#include <algorithm>
#include <string>

#include "../core/I18n.h"
#include "../core/Logger.h"
#include "Resource.h"

namespace flexwin {

namespace {
constexpr wchar_t kLogDlgClass[] = L"FlexWinLogDlg";
constexpr int IDC_LOG_EDIT = 401;
constexpr int IDC_LOG_REFRESH = 402;
constexpr int IDC_LOG_CLEAR = 403;
constexpr int IDC_LOG_CLOSE = 404;

int DpiScale(int px, UINT dpi) {
    return MulDiv(px, static_cast<int>(dpi), 96);
}
} // namespace

bool LogDialog::Register(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = LogDialog::WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kLogDlgClass;
    return RegisterClassExW(&wc) != 0;
}

void LogDialog::Show(HWND owner, HINSTANCE hInst, HFONT font) {
    m_font = font;
    if (m_hwnd && IsWindow(m_hwnd)) {
        RefreshLanguage();
        Refresh();
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        return;
    }

    m_dpi = GetDpiForWindow(owner);
    if (m_dpi == 0) m_dpi = 96;

    const int w = DpiScale(700, m_dpi);
    const int h = DpiScale(420, m_dpi);
    RECT ownerRc{};
    GetWindowRect(owner, &ownerRc);
    const int x = ownerRc.left + ((ownerRc.right - ownerRc.left) - w) / 2;
    const int y = ownerRc.top + ((ownerRc.bottom - ownerRc.top) - h) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME, kLogDlgClass, Tr(StrKey::LogTitle),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, x, y, w, h, owner, nullptr, hInst, this);
}

void LogDialog::Close() {
    if (m_hwnd && IsWindow(m_hwnd)) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

LRESULT CALLBACK LogDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = static_cast<LogDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->m_hwnd = hwnd;
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    auto* self = reinterpret_cast<LogDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT LogDialog::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hInst = GetModuleHandleW(nullptr);
        m_edit = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 100, 100, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LOG_EDIT)), hInst, nullptr);
        if (m_font) SendMessageW(m_edit, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);

        auto makeBtn = [&](const wchar_t* txt, int id) {
            HWND b = CreateWindowExW(0, L"BUTTON", txt,
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                     0, 0, 90, 28, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                     hInst, nullptr);
            if (m_font) SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
            return b;
        };
        m_btnRefresh = makeBtn(Tr(StrKey::LogRefresh), IDC_LOG_REFRESH);
        m_btnClear = makeBtn(Tr(StrKey::LogClear), IDC_LOG_CLEAR);
        m_btnClose = makeBtn(Tr(StrKey::LogClose), IDC_LOG_CLOSE);

        RECT rc{};
        GetClientRect(m_hwnd, &rc);
        LayoutControls(rc.right - rc.left, rc.bottom - rc.top);
        Refresh();
        return 0;
    }
    case WM_SIZE:
        LayoutControls(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_LOG_REFRESH: Refresh(); break;
        case IDC_LOG_CLEAR: Logger::Instance().Clear(); Refresh(); break;
        case IDC_LOG_CLOSE: DestroyWindow(m_hwnd); break;
        }
        return 0;
    case WM_DESTROY:
        m_hwnd = nullptr;
        return 0;
    default:
        return DefWindowProcW(m_hwnd, msg, wParam, lParam);
    }
}

void LogDialog::LayoutControls(int cw, int ch) {
    const int pad = DpiScale(10, m_dpi);
    const int btnH = DpiScale(30, m_dpi);
    const int editH = (std::max)(80, ch - pad * 3 - btnH);

    MoveWindow(m_edit, pad, pad, (std::max)(100, cw - pad * 2), editH, TRUE);
    const int by = pad * 2 + editH;
    const int bw = DpiScale(95, m_dpi);
    MoveWindow(m_btnRefresh, pad, by, bw, btnH, TRUE);
    MoveWindow(m_btnClear, pad * 2 + bw, by, bw, btnH, TRUE);
    MoveWindow(m_btnClose, cw - pad - bw, by, bw, btnH, TRUE);
}

void LogDialog::RefreshLanguage() {
    if (!m_hwnd || !IsWindow(m_hwnd)) return;
    SetWindowTextW(m_hwnd, Tr(StrKey::LogTitle));
    SetWindowTextW(m_btnRefresh, Tr(StrKey::LogRefresh));
    SetWindowTextW(m_btnClear, Tr(StrKey::LogClear));
    SetWindowTextW(m_btnClose, Tr(StrKey::LogClose));
    Refresh();
}

void LogDialog::Refresh() {
    if (!m_edit) return;
    const auto entries = Logger::Instance().Snapshot();
    std::wstring text;
    text.reserve(entries.size() * 96);
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        const wchar_t* tag = Tr(StrKey::LogTagInfo);
        switch (it->level) {
        case LogLevel::Success: tag = Tr(StrKey::LogTagOk); break;
        case LogLevel::Warning: tag = Tr(StrKey::LogTagWarn); break;
        case LogLevel::Error:   tag = Tr(StrKey::LogTagErr); break;
        default: break;
        }
        text += it->timestamp + L" " + tag + it->message + L"\r\n";
    }
    SetWindowTextW(m_edit, text.c_str());
}

} // namespace flexwin
