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

#include "../core/WindowInspector.h"

namespace flexwin {

class OpenWindowsDialog {
public:
    static bool Register(HINSTANCE hInst);

    void Show(HWND owner, HINSTANCE hInst, HFONT font);
    void Refresh();
    void RefreshLanguage();
    void Close();

    void SetOnCreateRule(std::function<void(const WindowInfo&)> cb) { m_onCreateRule = std::move(cb); }
    void SetOnQuickUnlock(std::function<void(const WindowInfo&)> cb) { m_onQuickUnlock = std::move(cb); }
    void SetOnQuickMinimize(std::function<void(const WindowInfo&)> cb) { m_onQuickMinimize = std::move(cb); }
    void SetOnQuickClose(std::function<void(const WindowInfo&)> cb) { m_onQuickClose = std::move(cb); }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void LayoutControls(int cw, int ch);
    const WindowInfo* GetSelectedInfo() const;

    HWND m_hwnd = nullptr;
    HWND m_list = nullptr;
    HWND m_btnRefresh = nullptr;
    HWND m_btnCreateRule = nullptr;
    HWND m_btnQuickUnlock = nullptr;
    HWND m_btnQuickMin = nullptr;
    HWND m_btnQuickClose = nullptr;
    HWND m_btnClose = nullptr;
    HFONT m_font = nullptr;
    UINT m_dpi = 96;

    std::vector<WindowInfo> m_windows;
    std::function<void(const WindowInfo&)> m_onCreateRule;
    std::function<void(const WindowInfo&)> m_onQuickUnlock;
    std::function<void(const WindowInfo&)> m_onQuickMinimize;
    std::function<void(const WindowInfo&)> m_onQuickClose;
};

} // namespace flexwin
