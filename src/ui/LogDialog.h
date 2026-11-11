#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

namespace flexwin {

class LogDialog {
public:
    static bool Register(HINSTANCE hInst);
    void Show(HWND owner, HINSTANCE hInst, HFONT font);
    void Refresh();
    void RefreshLanguage();
    void Close();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    void LayoutControls(int cw, int ch);

    HWND m_hwnd = nullptr;
    HWND m_edit = nullptr;
    HWND m_btnRefresh = nullptr;
    HWND m_btnClear = nullptr;
    HWND m_btnClose = nullptr;
    HFONT m_font = nullptr;
    UINT m_dpi = 96;
};

} // namespace flexwin
