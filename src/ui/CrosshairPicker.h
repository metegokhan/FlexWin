#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <functional>

namespace flexwin {

class CrosshairPicker {
public:
    static bool Register(HINSTANCE hInst);

    HWND Create(HWND parent, HINSTANCE hInst, int x, int y, int w, int h, int ctrlId);
    HWND GetHwnd() const { return m_hwnd; }
    void SetFont(HFONT font);
    void RefreshLanguage() { if (m_hwnd) InvalidateRect(m_hwnd, nullptr, TRUE); }

    void SetOnHover(std::function<void(HWND)> cb) { m_onHover = std::move(cb); }
    void SetOnPicked(std::function<void(HWND)> cb) { m_onPicked = std::move(cb); }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void BeginPick();
    void EndPick(bool commit);
    void UpdateTargetAtCursor();
    void ShowHighlightRect(const RECT& r);
    void HideHighlight();

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInst = nullptr;
    HICON m_icon = nullptr;
    HCURSOR m_crossCursor = nullptr;
    HFONT m_font = nullptr;

    bool m_dragging = false;
    HWND m_hoveredHwnd = nullptr;
    HWND m_borderTop = nullptr;
    HWND m_borderBottom = nullptr;
    HWND m_borderLeft = nullptr;
    HWND m_borderRight = nullptr;

    std::function<void(HWND)> m_onHover;
    std::function<void(HWND)> m_onPicked;
};

} // namespace flexwin
