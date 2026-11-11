#include "OpenWindowsDialog.h"

#include <CommCtrl.h>
#include <algorithm>
#include "../core/I18n.h"
#include "Resource.h"

namespace flexwin {

namespace {

constexpr wchar_t kOpenWinsClass[] = L"FlexWinOpenWindowsDlg";
constexpr int IDC_OW_LIST = 301;
constexpr int IDC_OW_REFRESH = 302;
constexpr int IDC_OW_CREATE_RULE = 303;
constexpr int IDC_OW_QUICK_UNLOCK = 304;
constexpr int IDC_OW_QUICK_MIN = 305;
constexpr int IDC_OW_QUICK_CLOSE = 306;
constexpr int IDC_OW_CLOSE = 307;

int DpiScale(int px, UINT dpi) {
    return MulDiv(px, static_cast<int>(dpi), 96);
}

} // namespace

bool OpenWindowsDialog::Register(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OpenWindowsDialog::WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kOpenWinsClass;
    return RegisterClassExW(&wc) != 0;
}

void OpenWindowsDialog::Show(HWND owner, HINSTANCE hInst, HFONT font) {
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

    const int w = DpiScale(900, m_dpi);
    const int h = DpiScale(480, m_dpi);
    RECT ownerRc{};
    GetWindowRect(owner, &ownerRc);
    const int x = ownerRc.left + ((ownerRc.right - ownerRc.left) - w) / 2;
    const int y = ownerRc.top + ((ownerRc.bottom - ownerRc.top) - h) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kOpenWinsClass,
        Tr(StrKey::OwTitle),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, w, h,
        owner, nullptr, hInst, this);
}

void OpenWindowsDialog::Close() {
    if (m_hwnd && IsWindow(m_hwnd)) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

LRESULT CALLBACK OpenWindowsDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = static_cast<OpenWindowsDialog*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->m_hwnd = hwnd;
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    auto* self = reinterpret_cast<OpenWindowsDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT OpenWindowsDialog::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hInst = GetModuleHandleW(nullptr);
        m_list = CreateWindowExW(
            WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 0, 100, 100, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_OW_LIST)), hInst, nullptr);
        ListView_SetExtendedListViewStyle(m_list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

        auto addCol = [&](int idx, const wchar_t* text, int widthPx) {
            LVCOLUMNW col{};
            col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            col.pszText = const_cast<wchar_t*>(text);
            col.cx = DpiScale(widthPx, m_dpi);
            col.iSubItem = idx;
            ListView_InsertColumn(m_list, idx, &col);
        };
        addCol(0, Tr(StrKey::OwColProc), 140);
        addCol(1, Tr(StrKey::OwColTitle), 250);
        addCol(2, Tr(StrKey::OwColClass), 160);
        addCol(3, Tr(StrKey::OwColSize), 95);
        addCol(4, Tr(StrKey::OwColResize), 80);
        addCol(5, Tr(StrKey::OwColArch), 120);

        auto makeBtn = [&](const wchar_t* text, int id) {
            HWND b = CreateWindowExW(0, L"BUTTON", text,
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                     0, 0, 100, 28, m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                     hInst, nullptr);
            if (m_font) SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
            return b;
        };
        if (m_font) SendMessageW(m_list, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);

        m_btnRefresh = makeBtn(Tr(StrKey::OwRefresh), IDC_OW_REFRESH);
        m_btnCreateRule = makeBtn(Tr(StrKey::OwCreateRule), IDC_OW_CREATE_RULE);
        m_btnQuickUnlock = makeBtn(Tr(StrKey::OwQuickUnlock), IDC_OW_QUICK_UNLOCK);
        m_btnQuickMin = makeBtn(Tr(StrKey::OwQuickMinimize), IDC_OW_QUICK_MIN);
        m_btnQuickClose = makeBtn(Tr(StrKey::OwQuickClose), IDC_OW_QUICK_CLOSE);
        m_btnClose = makeBtn(Tr(StrKey::OwClose), IDC_OW_CLOSE);

        RECT rc{};
        GetClientRect(m_hwnd, &rc);
        LayoutControls(rc.right - rc.left, rc.bottom - rc.top);
        Refresh();
        return 0;
    }
    case WM_SIZE:
        LayoutControls(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        if (id == IDC_OW_REFRESH) {
            Refresh();
        } else if (id == IDC_OW_CREATE_RULE) {
            if (const WindowInfo* info = GetSelectedInfo(); info && m_onCreateRule) {
                m_onCreateRule(*info);
                DestroyWindow(m_hwnd);
            }
        } else if (id == IDC_OW_QUICK_UNLOCK) {
            if (const WindowInfo* info = GetSelectedInfo(); info && m_onQuickUnlock) {
                m_onQuickUnlock(*info);
                Refresh();
            }
        } else if (id == IDC_OW_QUICK_MIN) {
            if (const WindowInfo* info = GetSelectedInfo(); info && m_onQuickMinimize) {
                m_onQuickMinimize(*info);
                Refresh();
            }
        } else if (id == IDC_OW_QUICK_CLOSE) {
            if (const WindowInfo* info = GetSelectedInfo(); info && m_onQuickClose) {
                m_onQuickClose(*info);
                Refresh();
            }
        } else if (id == IDC_OW_CLOSE) {
            DestroyWindow(m_hwnd);
        }
        return 0;
    }
    case WM_NOTIFY: {
        auto* nm = reinterpret_cast<NMHDR*>(lParam);
        if (nm->idFrom == IDC_OW_LIST && nm->code == NM_DBLCLK) {
            if (const WindowInfo* info = GetSelectedInfo(); info && m_onCreateRule) {
                m_onCreateRule(*info);
                DestroyWindow(m_hwnd);
            }
        }
        return 0;
    }
    case WM_DESTROY:
        m_hwnd = nullptr;
        return 0;
    default:
        return DefWindowProcW(m_hwnd, msg, wParam, lParam);
    }
}

void OpenWindowsDialog::LayoutControls(int cw, int ch) {
    const int pad = DpiScale(10, m_dpi);
    const int btnH = DpiScale(30, m_dpi);
    const int listH = (std::max)(80, ch - pad * 3 - btnH);

    MoveWindow(m_list, pad, pad, (std::max)(100, cw - pad * 2), listH, TRUE);

    const int by = pad * 2 + listH;
    int x = pad;
    const int wRef = DpiScale(84, m_dpi);
    const int wCre = DpiScale(210, m_dpi);
    const int wUnl = DpiScale(136, m_dpi);
    const int wMin = DpiScale(90, m_dpi);
    const int wQCls = DpiScale(90, m_dpi);
    const int wCls = DpiScale(84, m_dpi);

    MoveWindow(m_btnRefresh, x, by, wRef, btnH, TRUE); x += wRef + pad;
    MoveWindow(m_btnCreateRule, x, by, wCre, btnH, TRUE); x += wCre + pad;
    MoveWindow(m_btnQuickUnlock, x, by, wUnl, btnH, TRUE); x += wUnl + pad;
    MoveWindow(m_btnQuickMin, x, by, wMin, btnH, TRUE); x += wMin + pad;
    MoveWindow(m_btnQuickClose, x, by, wQCls, btnH, TRUE);
    MoveWindow(m_btnClose, cw - pad - wCls, by, wCls, btnH, TRUE);
}

void OpenWindowsDialog::RefreshLanguage() {
    if (!m_hwnd || !IsWindow(m_hwnd)) return;
    SetWindowTextW(m_hwnd, Tr(StrKey::OwTitle));
    SetWindowTextW(m_btnRefresh, Tr(StrKey::OwRefresh));
    SetWindowTextW(m_btnCreateRule, Tr(StrKey::OwCreateRule));
    SetWindowTextW(m_btnQuickUnlock, Tr(StrKey::OwQuickUnlock));
    SetWindowTextW(m_btnQuickMin, Tr(StrKey::OwQuickMinimize));
    SetWindowTextW(m_btnQuickClose, Tr(StrKey::OwQuickClose));
    SetWindowTextW(m_btnClose, Tr(StrKey::OwClose));

    const StrKey cols[6] = {
        StrKey::OwColProc, StrKey::OwColTitle, StrKey::OwColClass,
        StrKey::OwColSize, StrKey::OwColResize, StrKey::OwColArch
    };
    for (int i = 0; i < 6; ++i) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT;
        col.pszText = const_cast<wchar_t*>(Tr(cols[i]));
        ListView_SetColumn(m_list, i, &col);
    }
    Refresh();
}

void OpenWindowsDialog::Refresh() {
    if (!m_list) return;
    ListView_DeleteAllItems(m_list);
    m_windows = WindowInspector::EnumerateTopLevelWindows(false);

    for (size_t i = 0; i < m_windows.size(); ++i) {
        const auto& w = m_windows[i];
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        std::wstring proc = w.processName.empty() ? Tr(StrKey::OwUnknown) : w.processName;
        item.pszText = const_cast<wchar_t*>(proc.c_str());
        ListView_InsertItem(m_list, &item);

        ListView_SetItemText(m_list, static_cast<int>(i), 1, const_cast<wchar_t*>(w.title.c_str()));
        ListView_SetItemText(m_list, static_cast<int>(i), 2, const_cast<wchar_t*>(w.className.c_str()));

        wchar_t dim[64]{};
        swprintf_s(dim, L"%ldx%ld",
                   w.visualRect.right - w.visualRect.left,
                   w.visualRect.bottom - w.visualRect.top);
        ListView_SetItemText(m_list, static_cast<int>(i), 3, dim);

        ListView_SetItemText(m_list, static_cast<int>(i), 4,
                             const_cast<wchar_t*>(w.isResizableStyle ? Tr(StrKey::OwResizable) : Tr(StrKey::OwLocked)));

        std::wstring arch = w.is32Bit ? L"x86" : L"x64";
        if (w.isElevated) arch += L" | Admin";
        ListView_SetItemText(m_list, static_cast<int>(i), 5, const_cast<wchar_t*>(arch.c_str()));
    }
}

const WindowInfo* OpenWindowsDialog::GetSelectedInfo() const {
    if (!m_list) return nullptr;
    int sel = ListView_GetNextItem(m_list, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= static_cast<int>(m_windows.size())) return nullptr;
    return &m_windows[static_cast<size_t>(sel)];
}

} // namespace flexwin
