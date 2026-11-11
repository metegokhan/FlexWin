#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../shared/HookProtocol.h"

namespace {

HMODULE g_hModule = nullptr;
UINT g_msgAttach = 0;
UINT g_msgDetach = 0;
UINT g_msgUpdate = 0;
INIT_ONCE g_msgInitOnce = INIT_ONCE_STATIC_INIT;

HANDLE g_hMap = nullptr;
const flexwin::SharedHookTable* g_table = nullptr;
DWORD g_tablePid = 0;
SRWLOCK g_mapLock = SRWLOCK_INIT;
ULONGLONG g_lastOpenAttemptTick = 0;

constexpr UINT_PTR kSubclassId = 0x46575343u; // 'FWSC'

struct PosChangeSnapshot {
    HWND hwnd;
    int cx;
    int cy;
    UINT flags;
};

constexpr int kPosStackCap = 16;
thread_local PosChangeSnapshot t_posStack[kPosStackCap]{};
thread_local int t_posDepth = 0;

BOOL CALLBACK InitMessagesOnce(PINIT_ONCE, PVOID, PVOID*) {
    g_msgAttach = RegisterWindowMessageW(flexwin::kHookAttachMsgName);
    g_msgDetach = RegisterWindowMessageW(flexwin::kHookDetachMsgName);
    g_msgUpdate = RegisterWindowMessageW(flexwin::kHookUpdateMsgName);
    return TRUE;
}

void EnsureMessages() {
    InitOnceExecuteOnce(&g_msgInitOnce, InitMessagesOnce, nullptr, nullptr);
}

void CloseSharedTableLocked() {
    if (g_table) {
        UnmapViewOfFile(g_table);
        g_table = nullptr;
    }
    if (g_hMap) {
        CloseHandle(g_hMap);
        g_hMap = nullptr;
    }
}

void SetTargetMainPid(DWORD pid) {
    if (pid == 0) return;
    AcquireSRWLockExclusive(&g_mapLock);
    if (g_tablePid != pid) {
        CloseSharedTableLocked();
        g_tablePid = pid;
        g_lastOpenAttemptTick = 0;
    }
    ReleaseSRWLockExclusive(&g_mapLock);
}

const flexwin::SharedHookTable* GetSharedTable() {
    AcquireSRWLockShared(&g_mapLock);
    const flexwin::SharedHookTable* tbl = g_table;
    const DWORD pid = g_tablePid;
    bool valid = false;
    if (tbl && pid != 0 && tbl->magic == flexwin::kHookTableMagic &&
        tbl->version == flexwin::kHookTableVersion) {
        const HWND mainHwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(tbl->mainHwnd));
        valid = (!mainHwnd || IsWindow(mainHwnd));
    }
    ReleaseSRWLockShared(&g_mapLock);
    if (valid) {
        return tbl;
    }

    AcquireSRWLockExclusive(&g_mapLock);
    if (g_tablePid == 0) {
        ReleaseSRWLockExclusive(&g_mapLock);
        return nullptr;
    }
    if (g_table) {
        const HWND mainHwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(g_table->mainHwnd));
        if (g_table->magic == flexwin::kHookTableMagic &&
            g_table->version == flexwin::kHookTableVersion &&
            (!mainHwnd || IsWindow(mainHwnd))) {
            tbl = g_table;
            ReleaseSRWLockExclusive(&g_mapLock);
            return tbl;
        }
        CloseSharedTableLocked();
    }

    const ULONGLONG now = GetTickCount64();
    if (now - g_lastOpenAttemptTick >= 50 || g_lastOpenAttemptTick == 0) {
        g_lastOpenAttemptTick = now;
        wchar_t mapName[96]{};
        swprintf_s(mapName, L"%s%lu", flexwin::kSharedTablePrefix, static_cast<unsigned long>(g_tablePid));
        g_hMap = OpenFileMappingW(FILE_MAP_READ, FALSE, mapName);
        if (g_hMap) {
            g_table = static_cast<const flexwin::SharedHookTable*>(
                MapViewOfFile(g_hMap, FILE_MAP_READ, 0, 0, sizeof(flexwin::SharedHookTable)));
            if (!g_table) {
                CloseHandle(g_hMap);
                g_hMap = nullptr;
            }
        }
    }
    tbl = g_table;
    ReleaseSRWLockExclusive(&g_mapLock);

    if (tbl && tbl->magic == flexwin::kHookTableMagic && tbl->version == flexwin::kHookTableVersion) {
        return tbl;
    }
    return nullptr;
}

bool LookupEntry(HWND hwnd, flexwin::SharedHookEntry& outEntry) {
    const flexwin::SharedHookTable* table = GetSharedTable();
    if (!table) {
        return false;
    }

    const HWND mainHwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(table->mainHwnd));
    if (mainHwnd && !IsWindow(mainHwnd)) {
        return false;
    }

    const uint64_t target = flexwin::HwndToU64(hwnd);
    for (uint32_t i = 0; i < flexwin::kMaxHookEntries; ++i) {
        const flexwin::SharedHookEntry& slot = table->entries[i];
        if (slot.active == 0 || slot.hwnd != target) {
            continue;
        }

        for (int attempt = 0; attempt < 4; ++attempt) {
            const uint32_t seq1 = slot.seq;
            MemoryBarrier();
            if ((seq1 & 1u) != 0) {
                YieldProcessor();
                continue;
            }
            flexwin::SharedHookEntry copy{};
            std::memcpy(&copy, const_cast<const flexwin::SharedHookEntry*>(&slot), sizeof(copy));
            MemoryBarrier();
            const uint32_t seq2 = slot.seq;
            if (seq1 == seq2 && copy.active == 1 && copy.hwnd == target) {
                outEntry = copy;
                return true;
            }
        }
    }
    return false;
}

void ApplyMinMaxOverride(HWND hwnd, MINMAXINFO* mmi, const flexwin::SharedHookEntry& entry) {
    if (!mmi) return;

    const int minW = (entry.minWidth > 0) ? entry.minWidth : 120;
    const int minH = (entry.minHeight > 0) ? entry.minHeight : 80;
    const int maxW = (entry.maxWidth > 0) ? entry.maxWidth : 32767;
    const int maxH = (entry.maxHeight > 0) ? entry.maxHeight : 32767;

    mmi->ptMinTrackSize.x = minW;
    mmi->ptMinTrackSize.y = minH;
    mmi->ptMaxTrackSize.x = maxW;
    mmi->ptMaxTrackSize.y = maxH;

    if (entry.maxWidth <= 0 && entry.maxHeight <= 0) {
        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (hMon && GetMonitorInfoW(hMon, &mi)) {
            const LONG workW = mi.rcWork.right - mi.rcWork.left;
            const LONG workH = mi.rcWork.bottom - mi.rcWork.top;
            if (mmi->ptMaxSize.x > 0 && mmi->ptMaxSize.x < workW) {
                mmi->ptMaxSize.x = workW;
            }
            if (mmi->ptMaxSize.y > 0 && mmi->ptMaxSize.y < workH) {
                mmi->ptMaxSize.y = workH;
            }
        }
    }
}

LRESULT CALLBACK FlexWinSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                      UINT_PTR uIdSubclass, DWORD_PTR) {
    EnsureMessages();
    if (uMsg == g_msgDetach || uMsg == WM_NCDESTROY) {
        RemovePropW(hwnd, L"FlexWin_Sub");
        RemoveWindowSubclass(hwnd, FlexWinSubclassProc, uIdSubclass);
        return DefSubclassProc(hwnd, uMsg, wParam, lParam);
    }

    if (uMsg == WM_GETMINMAXINFO && lParam != 0) {
        const LRESULT res = DefSubclassProc(hwnd, uMsg, wParam, lParam);
        flexwin::SharedHookEntry entry{};
        if (!LookupEntry(hwnd, entry)) {
            RemovePropW(hwnd, L"FlexWin_Sub");
            RemoveWindowSubclass(hwnd, FlexWinSubclassProc, uIdSubclass);
            return res;
        }
        if (entry.flags & flexwin::kHookFlagUnlockMinMax) {
            ApplyMinMaxOverride(hwnd, reinterpret_cast<MINMAXINFO*>(lParam), entry);
        }
        return res;
    }

    if (uMsg == WM_WINDOWPOSCHANGING && lParam != 0) {
        WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
        const int origCx = wp->cx;
        const int origCy = wp->cy;
        const UINT origFlags = wp->flags;

        const LRESULT res = DefSubclassProc(hwnd, uMsg, wParam, lParam);
        flexwin::SharedHookEntry entry{};
        if (!LookupEntry(hwnd, entry)) {
            RemovePropW(hwnd, L"FlexWin_Sub");
            RemoveWindowSubclass(hwnd, FlexWinSubclassProc, uIdSubclass);
            return res;
        }
        if (!IsIconic(hwnd)) {
            if ((entry.flags & flexwin::kHookFlagForceExactSize) &&
                entry.targetWidth > 0 && entry.targetHeight > 0) {
                wp->cx = entry.targetWidth;
                wp->cy = entry.targetHeight;
                wp->flags &= ~SWP_NOSIZE;
            } else if ((entry.flags & flexwin::kHookFlagAllowResize) && !(origFlags & SWP_NOSIZE)) {
                wp->cx = origCx;
                wp->cy = origCy;
                wp->flags &= ~SWP_NOSIZE;
            }
        }
        return res;
    }

    if (uMsg == WM_STYLECHANGING && wParam == GWL_STYLE && lParam != 0) {
        const LRESULT res = DefSubclassProc(hwnd, uMsg, wParam, lParam);
        flexwin::SharedHookEntry entry{};
        if (LookupEntry(hwnd, entry) && (entry.flags & flexwin::kHookFlagKeepThickFrame)) {
            STYLESTRUCT* ss = reinterpret_cast<STYLESTRUCT*>(lParam);
            ss->styleNew |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
        }
        return res;
    }

    if (uMsg == WM_NCHITTEST) {
        flexwin::SharedHookEntry entry{};
        if (!LookupEntry(hwnd, entry)) {
            RemovePropW(hwnd, L"FlexWin_Sub");
            RemoveWindowSubclass(hwnd, FlexWinSubclassProc, uIdSubclass);
            return DefSubclassProc(hwnd, uMsg, wParam, lParam);
        }

        const LRESULT hit = DefSubclassProc(hwnd, uMsg, wParam, lParam);
        if ((entry.flags & flexwin::kHookFlagUnlockHitTest) &&
            !IsZoomed(hwnd) && !IsIconic(hwnd) &&
            (hit == HTCLIENT || hit == HTBORDER || hit == HTNOWHERE || hit == HTCAPTION)) {
            RECT rc{};
            if (GetWindowRect(hwnd, &rc)) {
                const int px = GET_X_LPARAM(lParam);
                const int py = GET_Y_LPARAM(lParam);
                const int borderX = (std::max)(8, GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER));
                const int borderY = (std::max)(8, GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER));

                const bool left = (px >= rc.left && px < rc.left + borderX);
                const bool right = (px <= rc.right && px > rc.right - borderX);
                const bool top = (py >= rc.top && py < rc.top + borderY);
                const bool bottom = (py <= rc.bottom && py > rc.bottom - borderY);

                if (top && left) return HTTOPLEFT;
                if (top && right) return HTTOPRIGHT;
                if (bottom && left) return HTBOTTOMLEFT;
                if (bottom && right) return HTBOTTOMRIGHT;
                if (left) return HTLEFT;
                if (right) return HTRIGHT;
                if (top) return HTTOP;
                if (bottom) return HTBOTTOM;
            }
        }
        return hit;
    }

    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

void EnsureWindowSubclass(HWND hwnd, bool attach) {
    if (!IsWindow(hwnd)) return;
    if (attach) {
        if (GetPropW(hwnd, L"FlexWin_Sub") != nullptr) return;
        static LONG s_pinnedForSubclass = 0;
        if (InterlockedCompareExchange(&s_pinnedForSubclass, 1, 0) == 0) {
            HMODULE hPin = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                               reinterpret_cast<LPCWSTR>(&FlexWinSubclassProc), &hPin);
        }
        if (SetWindowSubclass(hwnd, FlexWinSubclassProc, kSubclassId, 0)) {
            SetPropW(hwnd, L"FlexWin_Sub", reinterpret_cast<HANDLE>(1));
        }
    } else {
        RemovePropW(hwnd, L"FlexWin_Sub");
        RemoveWindowSubclass(hwnd, FlexWinSubclassProc, kSubclassId);
    }
}

} // namespace

extern "C" {

LRESULT CALLBACK FlexWinCallWndProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && lParam != 0) {
        EnsureMessages();
        const CWPSTRUCT* cwp = reinterpret_cast<const CWPSTRUCT*>(lParam);
        if (cwp->message == g_msgAttach || cwp->message == g_msgUpdate) {
            if (cwp->wParam != 0) {
                SetTargetMainPid(static_cast<DWORD>(cwp->wParam));
            }
            flexwin::SharedHookEntry entry{};
            if (LookupEntry(cwp->hwnd, entry) && entry.flags != flexwin::kHookFlagNone) {
                EnsureWindowSubclass(cwp->hwnd, true);
            }
        } else if (cwp->message == g_msgDetach) {
            EnsureWindowSubclass(cwp->hwnd, false);
        } else if (cwp->message == WM_WINDOWPOSCHANGING && cwp->lParam != 0) {
            flexwin::SharedHookEntry entry{};
            if (LookupEntry(cwp->hwnd, entry)) {
                EnsureWindowSubclass(cwp->hwnd, true);
                if (entry.flags & (flexwin::kHookFlagAllowResize | flexwin::kHookFlagForceExactSize)) {
                    if (t_posDepth < kPosStackCap) {
                        const WINDOWPOS* wp = reinterpret_cast<const WINDOWPOS*>(cwp->lParam);
                        t_posStack[t_posDepth++] = PosChangeSnapshot{cwp->hwnd, wp->cx, wp->cy, wp->flags};
                    }
                }
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK FlexWinCallWndRetProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && lParam != 0) {
        EnsureMessages();
        const CWPRETSTRUCT* cwpret = reinterpret_cast<const CWPRETSTRUCT*>(lParam);
        const UINT msg = cwpret->message;

        if (msg == WM_GETMINMAXINFO && cwpret->lParam != 0) {
            flexwin::SharedHookEntry entry{};
            if (LookupEntry(cwpret->hwnd, entry) && (entry.flags & flexwin::kHookFlagUnlockMinMax)) {
                ApplyMinMaxOverride(cwpret->hwnd, reinterpret_cast<MINMAXINFO*>(cwpret->lParam), entry);
            }
        } else if (msg == WM_WINDOWPOSCHANGING && cwpret->lParam != 0) {
            PosChangeSnapshot snap{};
            bool hasSnap = false;
            for (int i = t_posDepth - 1; i >= 0; --i) {
                if (t_posStack[i].hwnd == cwpret->hwnd) {
                    snap = t_posStack[i];
                    t_posDepth = i;
                    hasSnap = true;
                    break;
                }
            }

            flexwin::SharedHookEntry entry{};
            if (LookupEntry(cwpret->hwnd, entry)) {
                WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(cwpret->lParam);
                if (!IsIconic(cwpret->hwnd)) {
                    if ((entry.flags & flexwin::kHookFlagForceExactSize) &&
                        entry.targetWidth > 0 && entry.targetHeight > 0) {
                        wp->cx = entry.targetWidth;
                        wp->cy = entry.targetHeight;
                        wp->flags &= ~SWP_NOSIZE;
                    } else if (hasSnap && (entry.flags & flexwin::kHookFlagAllowResize) &&
                               !(snap.flags & SWP_NOSIZE)) {
                        wp->cx = snap.cx;
                        wp->cy = snap.cy;
                        wp->flags &= ~SWP_NOSIZE;
                    }
                }
            }
        } else if (msg == WM_STYLECHANGING && cwpret->wParam == GWL_STYLE && cwpret->lParam != 0) {
            flexwin::SharedHookEntry entry{};
            if (LookupEntry(cwpret->hwnd, entry) && (entry.flags & flexwin::kHookFlagKeepThickFrame)) {
                STYLESTRUCT* ss = reinterpret_cast<STYLESTRUCT*>(cwpret->lParam);
                ss->styleNew |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

} // extern "C"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
    } else if (reason == DLL_PROCESS_DETACH) {
        AcquireSRWLockExclusive(&g_mapLock);
        CloseSharedTableLocked();
        ReleaseSRWLockExclusive(&g_mapLock);
    }
    return TRUE;
}
