#include "MonitorPreview.h"

#include <windowsx.h>
#include <algorithm>
#include <cmath>

#include "../core/I18n.h"

namespace flexwin {

namespace {
constexpr wchar_t kMonitorPreviewClass[] = L"FlexWinMonitorPreview";
} // namespace

bool MonitorPreview::Register(HINSTANCE hInst) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MonitorPreview::WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kMonitorPreviewClass;
    return RegisterClassExW(&wc) != 0;
}

HWND MonitorPreview::Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int ctrlId) {
    m_monitors = MonitorManager::EnumerateMonitors();
    m_hwnd = CreateWindowExW(
        0, kMonitorPreviewClass, L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ctrlId)),
        hInst, this);
    return m_hwnd;
}

void MonitorPreview::SetFont(HFONT font) {
    m_font = font;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

void MonitorPreview::SetRulePreview(const WindowRule& rule, const std::vector<MonitorInfoEx>& monitors) {
    m_rule = rule;
    m_monitors = monitors;
    if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE);
}

LRESULT CALLBACK MonitorPreview::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* self = static_cast<MonitorPreview*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->m_hwnd = hwnd;
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    auto* self = reinterpret_cast<MonitorPreview*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) return self->HandleMessage(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MonitorPreview::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(m_hwnd, &ps);
        RECT rc{};
        GetClientRect(m_hwnd, &rc);

        const int w = (std::max)(1L, rc.right - rc.left);
        const int h = (std::max)(1L, rc.bottom - rc.top);
        HDC memDc = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDc, bmp));

        Paint(memDc, rc);
        BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);

        SelectObject(memDc, oldBmp);
        DeleteObject(bmp);
        DeleteDC(memDc);
        EndPaint(m_hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        for (size_t i = 0; i < m_drawnMonitorRects.size(); ++i) {
            if (PtInRect(&m_drawnMonitorRects[i], pt)) {
                if (m_onMonitorClicked) m_onMonitorClicked(static_cast<int>(i));
                break;
            }
        }
        return 0;
    }
    default:
        return DefWindowProcW(m_hwnd, msg, wParam, lParam);
    }
}

void MonitorPreview::Paint(HDC hdc, const RECT& client) {
    HBRUSH canvasBr = CreateSolidBrush(RGB(26, 33, 46));
    FillRect(hdc, &client, canvasBr);
    DeleteObject(canvasBr);

    m_drawnMonitorRects.clear();
    if (m_monitors.empty()) return;

    LONG minX = m_monitors[0].monitorRect.left;
    LONG minY = m_monitors[0].monitorRect.top;
    LONG maxX = m_monitors[0].monitorRect.right;
    LONG maxY = m_monitors[0].monitorRect.bottom;
    for (const auto& m : m_monitors) {
        minX = (std::min)(minX, m.monitorRect.left);
        minY = (std::min)(minY, m.monitorRect.top);
        maxX = (std::max)(maxX, m.monitorRect.right);
        maxY = (std::max)(maxY, m.monitorRect.bottom);
    }

    const double vW = static_cast<double>((std::max)(1L, maxX - minX));
    const double vH = static_cast<double>((std::max)(1L, maxY - minY));
    const int pad = 12;
    const double availW = static_cast<double>((std::max)(20L, (client.right - client.left) - pad * 2));
    const double availH = static_cast<double>((std::max)(20L, (client.bottom - client.top) - pad * 2));
    const double scale = (std::min)(availW / vW, availH / vH);

    const int drawW = static_cast<int>(std::lround(vW * scale));
    const int drawH = static_cast<int>(std::lround(vH * scale));
    const int offX = ((client.right - client.left) - drawW) / 2;
    const int offY = ((client.bottom - client.top) - drawH) / 2;

    auto toPreview = [&](const RECT& r) -> RECT {
        return RECT{
            offX + static_cast<int>(std::lround((r.left - minX) * scale)),
            offY + static_cast<int>(std::lround((r.top - minY) * scale)),
            offX + static_cast<int>(std::lround((r.right - minX) * scale)),
            offY + static_cast<int>(std::lround((r.bottom - minY) * scale))
        };
    };

    int targetMonIdx = MonitorManager::FindPrimaryMonitorIndex(m_monitors);
    if (m_rule.monitorMode == MonitorTargetMode::Index) {
        targetMonIdx = std::clamp(m_rule.monitorIndex, 0, static_cast<int>(m_monitors.size()) - 1);
    }

    HFONT oldFont = m_font ? static_cast<HFONT>(SelectObject(hdc, m_font)) : nullptr;
    SetBkMode(hdc, TRANSPARENT);

    for (size_t i = 0; i < m_monitors.size(); ++i) {
        RECT mr = toPreview(m_monitors[i].monitorRect);
        RECT wr = toPreview(m_monitors[i].workRect);
        m_drawnMonitorRects.push_back(mr);

        const bool selected = (static_cast<int>(i) == targetMonIdx);
        HBRUSH monBr = CreateSolidBrush(selected ? RGB(43, 55, 78) : RGB(36, 44, 60));
        FillRect(hdc, &mr, monBr);
        DeleteObject(monBr);

        HBRUSH workBr = CreateSolidBrush(selected ? RGB(52, 68, 96) : RGB(44, 54, 72));
        FillRect(hdc, &wr, workBr);
        DeleteObject(workBr);

        HPEN monPen = CreatePen(PS_SOLID, selected ? 2 : 1,
                                selected ? RGB(95, 165, 250) : RGB(80, 96, 125));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, monPen));
        HBRUSH nullBr = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
        HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, nullBr));
        Rectangle(hdc, mr.left, mr.top, mr.right, mr.bottom);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPen);
        DeleteObject(monPen);

        wchar_t label[64]{};
        swprintf_s(label, L"#%zu %ldx%ld%s", i + 1,
                   m_monitors[i].monitorRect.right - m_monitors[i].monitorRect.left,
                   m_monitors[i].monitorRect.bottom - m_monitors[i].monitorRect.top,
                   m_monitors[i].isPrimary ? L" *" : L"");
        SetTextColor(hdc, RGB(195, 210, 232));
        RECT txtRc = wr;
        txtRc.left += 4;
        txtRc.top += 3;
        DrawTextW(hdc, label, -1, &txtRc, DT_LEFT | DT_TOP | DT_SINGLELINE);
    }

    if (m_rule.action == WindowAction::AutoMinimize) {
        SetTextColor(hdc, RGB(255, 215, 130));
        RECT infoRc = client;
        infoRc.bottom -= 4;
        DrawTextW(hdc, Tr(StrKey::PreviewMinimize), -1, &infoRc, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
    } else if (m_rule.action == WindowAction::AutoClose) {
        SetTextColor(hdc, RGB(255, 145, 145));
        RECT infoRc = client;
        infoRc.bottom -= 4;
        DrawTextW(hdc, Tr(StrKey::PreviewClose), -1, &infoRc, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
    } else if (m_rule.geometryMode != GeometryMode::StyleOnly) {
        const RECT previewTarget = MonitorManager::ComputePreviewRect(m_rule, m_monitors, targetMonIdx);
        RECT pr = toPreview(previewTarget);

        HBRUSH zoneBr = CreateSolidBrush(RGB(14, 125, 215));
        FillRect(hdc, &pr, zoneBr);
        DeleteObject(zoneBr);

        HPEN zonePen = CreatePen(PS_SOLID, 2, RGB(125, 225, 255));
        HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, zonePen));
        HBRUSH oldBr = static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));
        Rectangle(hdc, pr.left, pr.top, pr.right, pr.bottom);
        SelectObject(hdc, oldBr);
        SelectObject(hdc, oldPen);
        DeleteObject(zonePen);

        wchar_t dimBuf[64]{};
        if (m_rule.enableOpacity) {
            swprintf_s(dimBuf, L"%ld x %ld (%d%%)",
                       previewTarget.right - previewTarget.left,
                       previewTarget.bottom - previewTarget.top,
                       m_rule.opacityPercent);
        } else {
            swprintf_s(dimBuf, L"%ld x %ld",
                       previewTarget.right - previewTarget.left,
                       previewTarget.bottom - previewTarget.top);
        }
        SetTextColor(hdc, RGB(245, 252, 255));
        DrawTextW(hdc, dimBuf, -1, &pr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        SetTextColor(hdc, RGB(150, 215, 255));
        RECT infoRc = client;
        infoRc.bottom -= 4;
        DrawTextW(hdc, Tr(StrKey::PreviewStyleOnly), -1, &infoRc,
                  DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
    }

    if (oldFont) SelectObject(hdc, oldFont);
}

} // namespace flexwin
