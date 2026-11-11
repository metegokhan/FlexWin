#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <functional>
#include <vector>

#include "../core/Models.h"
#include "../core/MonitorManager.h"

namespace flexwin {

class MonitorPreview {
public:
    static bool Register(HINSTANCE hInst);

    HWND Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int ctrlId);
    HWND GetHwnd() const { return m_hwnd; }
    void SetFont(HFONT font);
    void RefreshLanguage() { if (m_hwnd) InvalidateRect(m_hwnd, nullptr, FALSE); }
    void SetRulePreview(const WindowRule& rule, const std::vector<MonitorInfoEx>& monitors);
    void SetOnMonitorClicked(std::function<void(int monitorIndex)> cb) {
        m_onMonitorClicked = std::move(cb);
    }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void Paint(HDC hdc, const RECT& client);

    HWND m_hwnd = nullptr;
    HFONT m_font = nullptr;
    WindowRule m_rule;
    std::vector<MonitorInfoEx> m_monitors;
    std::vector<RECT> m_drawnMonitorRects;
    std::function<void(int)> m_onMonitorClicked;
};

} // namespace flexwin
