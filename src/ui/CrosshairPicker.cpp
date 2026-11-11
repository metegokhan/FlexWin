#include "CrosshairPicker.h"

#include <algorithm>

#include "../core/I18n.h"
#include "../core/MonitorManager.h"
#include "Resource.h"

namespace flexwin {

namespace {

constexpr wchar_t kCrosshairClass[] = L"FlexWinCrosshairPicker";
constexpr wchar_t kHighlightClass[] = L"FlexWinHighlightStrip";

LRESULT CALLBACK HighlightStripProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_ERASEBKGND) {
        HDC hdc = reinterpret_cast<HDC>(wParam);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        HBRUSH br = CreateSolidBrush(RGB(0, 140, 255));
        FillRect(hdc, &rc, br);
        DeleteObject(br);
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

bool CrosshairPicker::Register(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = CrosshairPicker::WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kCrosshairClass;
    RegisterClassExW(&wc);

    WNDCLASSEXW hwc{};
    hwc.cbSize = sizeof(hwc);
    hwc.lpfnWndProc = HighlightStripProc;
    hwc.hInstance = hInst;
    hwc.lpszClassName = kHighlightClass;
    RegisterClassExW(&hwc);

    return true;
}

HWND CrosshairPicker::Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int ctrlId) {
    m_hInst = hInst;
    m_icon = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(IDI_CROSSHAIR_ICON),
                                          IMAGE_ICON, 24, 24, LR_DEFAULTCOLOR));
    m_crossCursor = LoadCursorW(hInst, MAKEINTRESOURCEW(IDC_CROSSHAIR_CURSOR));
    if (!m_crossCursor) {
        m_crossCursor = LoadCursorW(nullptr, IDC_CROSS);
    }

    m_hwnd = CreateWindowExW(
        0, kCrosshairClass, L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ctrlId)),
        hInst, this);
    return m_hwnd;
}

void CrosshairPicker::SetFont(HFONT font) {
    m_font = font;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE);
}

LRESULT CALLBACK CrosshairPicker::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = static_cast<CrosshairPicker*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->m_hwnd = hwnd;
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    auto* self = reinterpret_cast<CrosshairPicker*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT CrosshairPicker::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(m_hwnd, &ps);
        RECT rc{};
        GetClientRect(m_hwnd, &rc);

        COLORREF bg = m_dragging ? RGB(215, 235, 255) : RGB(242, 246, 252);
        COLORREF border = m_dragging ? RGB(0, 120, 215) : RGB(140, 165, 200);

        HBRUSH bgBr = CreateSolidBrush(bg);
        FillRect(hdc, &rc, bgBr);
        DeleteObject(bgBr);

        HPEN pen = CreatePen(PS_SOLID, m_dragging ? 2 : 1, border);
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
        HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);

        const int h = rc.bottom - rc.top;
        const int pad = 8;
        if (m_icon && !m_dragging) {
            DrawIconEx(hdc, pad, (h - 24) / 2, m_icon, 24, 24, 0, nullptr, DI_NORMAL);
        }

        HFONT oldFont = m_font ? static_cast<HFONT>(SelectObject(hdc, m_font)) : nullptr;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(20, 45, 85));
        RECT textRc = rc;
        textRc.left += pad + (m_dragging ? 4 : 30);
        textRc.right -= pad;
        const wchar_t* label = m_dragging ? Tr(StrKey::PickerDragging) : Tr(StrKey::PickerReady);
        DrawTextW(hdc, label, -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (oldFont) SelectObject(hdc, oldFont);

        EndPaint(m_hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        BeginPick();
        return 0;
    case WM_MOUSEMOVE:
        if (m_dragging) {
            UpdateTargetAtCursor();
        }
        return 0;
    case WM_LBUTTONUP:
        if (m_dragging) {
            EndPick(true);
        }
        return 0;
    case WM_KEYDOWN:
        if (m_dragging && wParam == VK_ESCAPE) {
            EndPick(false);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (m_dragging) {
            EndPick(false);
        }
        return 0;
    case WM_DESTROY:
        HideHighlight();
        if (m_icon) {
            DestroyIcon(m_icon);
            m_icon = nullptr;
        }
        return 0;
    default:
        return DefWindowProcW(m_hwnd, msg, wParam, lParam);
    }
}

void CrosshairPicker::BeginPick() {
    m_dragging = true;
    m_hoveredHwnd = nullptr;
    SetFocus(m_hwnd);
    SetCapture(m_hwnd);
    if (m_crossCursor) SetCursor(m_crossCursor);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void CrosshairPicker::EndPick(bool commit) {
    if (!m_dragging) return;
    HWND picked = commit ? m_hoveredHwnd : nullptr;
    m_dragging = false;
    m_hoveredHwnd = nullptr;
    HideHighlight();
    if (GetCapture() == m_hwnd) {
        ReleaseCapture();
    }
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    InvalidateRect(m_hwnd, nullptr, TRUE);

    if (m_onPicked) {
        m_onPicked(picked);
    }
}

void CrosshairPicker::UpdateTargetAtCursor() {
    if (m_crossCursor) SetCursor(m_crossCursor);
    POINT pt{};
    if (!GetCursorPos(&pt)) return;

    HWND under = WindowFromPoint(pt);
    HWND root = under ? GetAncestor(under, GA_ROOT) : nullptr;
    if (!root) return;

    DWORD pid = 0;
    GetWindowThreadProcessId(root, &pid);
    if (pid == GetCurrentProcessId()) {
        if (m_hoveredHwnd != nullptr) {
            m_hoveredHwnd = nullptr;
            HideHighlight();
            if (m_onHover) m_onHover(nullptr);
        }
        return;
    }

    if (root != m_hoveredHwnd) {
        m_hoveredHwnd = root;
        const RECT r = MonitorManager::GetVisualWindowRect(root);
        ShowHighlightRect(r);
        if (m_onHover) m_onHover(root);
    }
}

void CrosshairPicker::ShowHighlightRect(const RECT& r) {
    const int thick = 4;
    const int w = (std::max)(8L, r.right - r.left);
    const int h = (std::max)(8L, r.bottom - r.top);

    auto ensureStrip = [&](HWND& strip) {
        if (!strip) {
            strip = CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
                kHighlightClass, L"", WS_POPUP,
                0, 0, 1, 1, nullptr, nullptr, m_hInst, nullptr);
        }
    };
    ensureStrip(m_borderTop);
    ensureStrip(m_borderBottom);
    ensureStrip(m_borderLeft);
    ensureStrip(m_borderRight);

    const UINT fl = SWP_NOACTIVATE | SWP_SHOWWINDOW;
    if (m_borderTop) SetWindowPos(m_borderTop, HWND_TOPMOST, r.left, r.top, w, thick, fl);
    if (m_borderBottom) SetWindowPos(m_borderBottom, HWND_TOPMOST, r.left, r.bottom - thick, w, thick, fl);
    if (m_borderLeft) SetWindowPos(m_borderLeft, HWND_TOPMOST, r.left, r.top, thick, h, fl);
    if (m_borderRight) SetWindowPos(m_borderRight, HWND_TOPMOST, r.right - thick, r.top, thick, h, fl);
}

void CrosshairPicker::HideHighlight() {
    auto destroyStrip = [](HWND& strip) {
        if (strip) {
            DestroyWindow(strip);
            strip = nullptr;
        }
    };
    destroyStrip(m_borderTop);
    destroyStrip(m_borderBottom);
    destroyStrip(m_borderLeft);
    destroyStrip(m_borderRight);
}

} // namespace flexwin
