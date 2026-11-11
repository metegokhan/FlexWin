#include "MonitorManager.h"

#include <dwmapi.h>
#include <ShellScalingApi.h>
#include <algorithm>
#include <cmath>

namespace flexwin {

namespace {

using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);

GetDpiForMonitorFn ResolveGetDpiForMonitor() {
    static GetDpiForMonitorFn fn = []() -> GetDpiForMonitorFn {
        HMODULE shcore = GetModuleHandleW(L"Shcore.dll");
        if (!shcore) shcore = LoadLibraryW(L"Shcore.dll");
        if (!shcore) return nullptr;
        return reinterpret_cast<GetDpiForMonitorFn>(GetProcAddress(shcore, "GetDpiForMonitor"));
    }();
    return fn;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM lParam) {
    auto* list = reinterpret_cast<std::vector<MonitorInfoEx>*>(lParam);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, &mi)) return TRUE;

    MonitorInfoEx info{};
    info.hMonitor = hMon;
    info.monitorRect = mi.rcMonitor;
    info.workRect = mi.rcWork;
    info.isPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    info.deviceName = mi.szDevice;

    if (auto fn = ResolveGetDpiForMonitor()) {
        UINT dx = 96, dy = 96;
        if (SUCCEEDED(fn(hMon, MDT_EFFECTIVE_DPI, &dx, &dy))) {
            info.dpiX = dx;
            info.dpiY = dy;
        }
    }
    list->push_back(std::move(info));
    return TRUE;
}

RECT ResolveWorkRect(const WindowRule& rule, const std::vector<MonitorInfoEx>& monitors, int windowMonIdx) {
    if (monitors.empty()) {
        RECT r{0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
        return r;
    }

    int idx = 0;
    switch (rule.monitorMode) {
    case MonitorTargetMode::WindowCurrent:
        idx = (windowMonIdx >= 0 && windowMonIdx < static_cast<int>(monitors.size()))
                  ? windowMonIdx
                  : MonitorManager::FindPrimaryMonitorIndex(monitors);
        break;
    case MonitorTargetMode::Primary:
        idx = MonitorManager::FindPrimaryMonitorIndex(monitors);
        break;
    case MonitorTargetMode::Index:
        idx = std::clamp(rule.monitorIndex, 0, static_cast<int>(monitors.size()) - 1);
        break;
    }
    return monitors[static_cast<size_t>(idx)].workRect;
}

RECT ComputeVisualBoundsForWorkRect(const WindowRule& rule, const RECT& work, const RECT& currentVisual) {
    const int workW = (std::max)(1L, work.right - work.left);
    const int workH = (std::max)(1L, work.bottom - work.top);
    RECT target = currentVisual;

    switch (rule.geometryMode) {
    case GeometryMode::Pixels: {
        const int w = (std::max)(50, rule.width);
        const int h = (std::max)(40, rule.height);
        target.left = work.left + rule.x;
        target.top = work.top + rule.y;
        target.right = target.left + w;
        target.bottom = target.top + h;
        break;
    }
    case GeometryMode::Percent: {
        const double xp = std::clamp(rule.xPct, -100.0, 200.0);
        const double yp = std::clamp(rule.yPct, -100.0, 200.0);
        const double wp = std::clamp(rule.widthPct, 5.0, 200.0);
        const double hp = std::clamp(rule.heightPct, 5.0, 200.0);
        target.left = work.left + static_cast<LONG>(std::lround(workW * (xp / 100.0)));
        target.top = work.top + static_cast<LONG>(std::lround(workH * (yp / 100.0)));
        target.right = target.left + (std::max)(50L, static_cast<LONG>(std::lround(workW * (wp / 100.0))));
        target.bottom = target.top + (std::max)(40L, static_cast<LONG>(std::lround(workH * (hp / 100.0))));
        break;
    }
    case GeometryMode::SizeOnlyPixels: {
        const int w = (std::max)(50, rule.width);
        const int h = (std::max)(40, rule.height);
        target.left = currentVisual.left;
        target.top = currentVisual.top;
        target.right = target.left + w;
        target.bottom = target.top + h;
        break;
    }
    case GeometryMode::PositionOnlyPixels: {
        const LONG curW = (std::max)(50L, currentVisual.right - currentVisual.left);
        const LONG curH = (std::max)(40L, currentVisual.bottom - currentVisual.top);
        target.left = work.left + rule.x;
        target.top = work.top + rule.y;
        target.right = target.left + curW;
        target.bottom = target.top + curH;
        break;
    }
    case GeometryMode::StyleOnly:
        break;
    }
    return target;
}

} // namespace

std::vector<MonitorInfoEx> MonitorManager::EnumerateMonitors() {
    std::vector<MonitorInfoEx> list;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&list));
    std::sort(list.begin(), list.end(), [](const MonitorInfoEx& a, const MonitorInfoEx& b) {
        if (a.monitorRect.left != b.monitorRect.left) {
            return a.monitorRect.left < b.monitorRect.left;
        }
        return a.monitorRect.top < b.monitorRect.top;
    });
    return list;
}

int MonitorManager::FindMonitorIndexForWindow(HWND hwnd, const std::vector<MonitorInfoEx>& monitors) {
    if (!hwnd || monitors.empty()) return 0;
    HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    for (size_t i = 0; i < monitors.size(); ++i) {
        if (monitors[i].hMonitor == hMon) return static_cast<int>(i);
    }
    return FindPrimaryMonitorIndex(monitors);
}

int MonitorManager::FindPrimaryMonitorIndex(const std::vector<MonitorInfoEx>& monitors) {
    for (size_t i = 0; i < monitors.size(); ++i) {
        if (monitors[i].isPrimary) return static_cast<int>(i);
    }
    return 0;
}

RECT MonitorManager::GetDwmInvisibleMargins(HWND hwnd) {
    RECT margins{0, 0, 0, 0};
    if (!hwnd || !IsWindow(hwnd) || IsIconic(hwnd)) return margins;

    RECT winRect{};
    RECT frameRect{};
    if (!GetWindowRect(hwnd, &winRect)) return margins;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frameRect, sizeof(frameRect)))) {
        margins.left = std::clamp(frameRect.left - winRect.left, 0L, 32L);
        margins.top = std::clamp(frameRect.top - winRect.top, 0L, 32L);
        margins.right = std::clamp(winRect.right - frameRect.right, 0L, 32L);
        margins.bottom = std::clamp(winRect.bottom - frameRect.bottom, 0L, 32L);
    }
    return margins;
}

RECT MonitorManager::GetVisualWindowRect(HWND hwnd) {
    RECT r{};
    if (!hwnd || !IsWindow(hwnd)) return r;
    if (!IsIconic(hwnd) &&
        SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r)))) {
        return r;
    }
    GetWindowRect(hwnd, &r);
    return r;
}

ComputedTargetRect MonitorManager::ComputeTargetRect(const WindowRule& rule, HWND hwnd,
                                                     const std::vector<MonitorInfoEx>& monitors) {
    ComputedTargetRect res{};
    if (rule.geometryMode == GeometryMode::StyleOnly) {
        return res;
    }

    const int curMon = FindMonitorIndexForWindow(hwnd, monitors);
    const RECT work = ResolveWorkRect(rule, monitors, curMon);
    const RECT curVisual = GetVisualWindowRect(hwnd);
    res.visualRect = ComputeVisualBoundsForWorkRect(rule, work, curVisual);

    res.move = (rule.geometryMode != GeometryMode::SizeOnlyPixels);
    res.resize = (rule.geometryMode != GeometryMode::PositionOnlyPixels);

    RECT margins{0, 0, 0, 0};
    if (rule.compensateDwmMargin && !rule.borderless) {
        margins = GetDwmInvisibleMargins(hwnd);
    }

    res.adjustedRect.left = res.visualRect.left - margins.left;
    res.adjustedRect.top = res.visualRect.top - margins.top;
    res.adjustedRect.right = res.visualRect.right + margins.right;
    res.adjustedRect.bottom = res.visualRect.bottom + margins.bottom;
    return res;
}

RECT MonitorManager::ComputePreviewRect(const WindowRule& rule,
                                        const std::vector<MonitorInfoEx>& monitors,
                                        int fallbackMonIndex) {
    if (monitors.empty()) return RECT{0, 0, 800, 600};
    int monIdx = fallbackMonIndex;
    if (rule.monitorMode == MonitorTargetMode::Primary) {
        monIdx = FindPrimaryMonitorIndex(monitors);
    } else if (rule.monitorMode == MonitorTargetMode::Index) {
        monIdx = std::clamp(rule.monitorIndex, 0, static_cast<int>(monitors.size()) - 1);
    } else {
        monIdx = std::clamp(fallbackMonIndex, 0, static_cast<int>(monitors.size()) - 1);
    }
    const RECT work = monitors[static_cast<size_t>(monIdx)].workRect;
    if (rule.geometryMode == GeometryMode::StyleOnly) {
        return work;
    }
    RECT fallbackCur{
        work.left + 80,
        work.top + 80,
        work.left + 80 + (std::max)(200, rule.width),
        work.top + 80 + (std::max)(150, rule.height)
    };
    return ComputeVisualBoundsForWorkRect(rule, work, fallbackCur);
}

} // namespace flexwin
