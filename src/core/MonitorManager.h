#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <string>
#include <vector>

#include "Models.h"

namespace flexwin {

struct MonitorInfoEx {
    HMONITOR hMonitor = nullptr;
    RECT monitorRect{};
    RECT workRect{};
    bool isPrimary = false;
    UINT dpiX = 96;
    UINT dpiY = 96;
    std::wstring deviceName;
};

struct ComputedTargetRect {
    bool move = false;
    bool resize = false;
    RECT visualRect{};     // Desired visible bounds
    RECT adjustedRect{};   // Compensated for DWM invisible borders (pass to SetWindowPos)
};

class MonitorManager {
public:
    static std::vector<MonitorInfoEx> EnumerateMonitors();
    static int FindMonitorIndexForWindow(HWND hwnd, const std::vector<MonitorInfoEx>& monitors);
    static int FindPrimaryMonitorIndex(const std::vector<MonitorInfoEx>& monitors);

    static RECT GetDwmInvisibleMargins(HWND hwnd);
    static RECT GetVisualWindowRect(HWND hwnd);

    static ComputedTargetRect ComputeTargetRect(const WindowRule& rule, HWND hwnd,
                                                const std::vector<MonitorInfoEx>& monitors);
    static RECT ComputePreviewRect(const WindowRule& rule,
                                   const std::vector<MonitorInfoEx>& monitors,
                                   int fallbackMonIndex = 0);
};

} // namespace flexwin
