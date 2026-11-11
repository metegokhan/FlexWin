#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "../../shared/HookProtocol.h"

namespace flexwin {

struct HookParams {
    uint32_t flags = 0;
    int32_t minWidth = 120;
    int32_t minHeight = 80;
    int32_t maxWidth = 0;
    int32_t maxHeight = 0;
    int32_t targetWidth = 0;
    int32_t targetHeight = 0;
};

class HookBridge {
public:
    HookBridge() = default;
    ~HookBridge();

    HookBridge(const HookBridge&) = delete;
    HookBridge& operator=(const HookBridge&) = delete;

    bool Initialize(HWND mainControllerHwnd);
    void Shutdown();

    bool AttachOrUpdate(HWND hwnd, DWORD threadId, bool is32Bit, const HookParams& params);
    void Detach(HWND hwnd);
    void DetachAll();
    void ReapDeadWindows();

    bool IsHook64Ready() const;
    bool IsHook32Available() const;
    bool IsHooked(HWND hwnd) const;
    size_t ActiveHookCount() const;

private:
    struct AttachedRecord {
        bool is32Bit = false;
        int slotIndex = -1;
        HHOOK callWnd = nullptr;
        HHOOK callWndRet = nullptr;
    };

    bool InitSharedMemory(HWND mainControllerHwnd);
    bool EnsureHelper32Running();
    HWND FindHelper32Window() const;
    int AllocateOrFindSlot(uint64_t hwndU64);
    void WriteSlot(int slotIdx, uint64_t hwndU64, bool active, const HookParams& params);
    void SendHookMessage(HWND hwnd, UINT msg);

    mutable std::mutex m_mutex;
    HANDLE m_hMap = nullptr;
    SharedHookTable* m_table = nullptr;

    HMODULE m_hook64Dll = nullptr;
    HOOKPROC m_callWndProc64 = nullptr;
    HOOKPROC m_callWndRetProc64 = nullptr;

    HANDLE m_helper32Proc = nullptr;
    bool m_helper32ExistsOnDisk = false;
    bool m_hook32ExistsOnDisk = false;

    UINT m_msgAttach = 0;
    UINT m_msgDetach = 0;
    UINT m_msgUpdate = 0;

    std::unordered_map<uint64_t, AttachedRecord> m_attached;
};

} // namespace flexwin
