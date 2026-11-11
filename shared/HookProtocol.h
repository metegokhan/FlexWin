#pragma once

#include <cstdint>

namespace flexwin {

constexpr wchar_t kSharedTablePrefix[] = L"Local\\FlexWin_HookTable_";
constexpr wchar_t kHookAttachMsgName[] = L"FlexWin_Hook_Attach_v1";
constexpr wchar_t kHookDetachMsgName[] = L"FlexWin_Hook_Detach_v1";
constexpr wchar_t kHookUpdateMsgName[] = L"FlexWin_Hook_Update_v1";

constexpr wchar_t kHelper32WindowClass[] = L"FlexWinHelper32_IPC_Wnd";
constexpr uint32_t kHelperCmdAttach = 0x46570001u;
constexpr uint32_t kHelperCmdDetach = 0x46570002u;
constexpr uint32_t kHelperCmdDetachAll = 0x46570003u;
constexpr uint32_t kHelperCmdShutdown = 0x46570004u;
constexpr uint32_t kHelperCmdPing = 0x46570005u;

constexpr uint32_t kMaxHookEntries = 256;
constexpr uint32_t kHookTableMagic = 0x46574854u; // 'FWHT'
constexpr uint32_t kHookTableVersion = 1;

enum HookRuleFlags : uint32_t {
    kHookFlagNone           = 0,
    kHookFlagUnlockMinMax   = 1u << 0, // Override WM_GETMINMAXINFO track limits
    kHookFlagAllowResize    = 1u << 1, // Restore size changes blocked in WM_WINDOWPOSCHANGING
    kHookFlagForceExactSize = 1u << 2, // Force targetWidth x targetHeight in WM_WINDOWPOSCHANGING
    kHookFlagUnlockHitTest  = 1u << 3, // Provide edge resize hit-test handles in WM_NCHITTEST
    kHookFlagKeepThickFrame = 1u << 4, // Prevent stripping WS_THICKFRAME in WM_STYLECHANGING
};

#pragma pack(push, 8)
struct SharedHookEntry {
    volatile uint32_t seq;  // Odd = writer in progress, Even = committed snapshot
    uint32_t active;        // 1 = slot occupied, 0 = empty
    uint64_t hwnd;          // Target HWND cast via (uint64_t)(uint32_t)(uintptr_t)
    uint32_t flags;         // Bitmask of HookRuleFlags
    int32_t minWidth;       // Minimum allowed width (<= 0 uses unlocked default)
    int32_t minHeight;      // Minimum allowed height (<= 0 uses unlocked default)
    int32_t maxWidth;       // Maximum allowed width (<= 0 uses unlocked default)
    int32_t maxHeight;      // Maximum allowed height (<= 0 uses unlocked default)
    int32_t targetWidth;    // Enforced width when kHookFlagForceExactSize is set
    int32_t targetHeight;   // Enforced height when kHookFlagForceExactSize is set
    uint32_t reserved;      // Pad struct size to 48 bytes (8-byte aligned)
};

struct SharedHookTable {
    uint32_t magic;
    uint32_t version;
    uint32_t mainPid;
    uint32_t reserved;
    uint64_t mainHwnd;      // Main FlexWin controller HWND for liveness checks
    SharedHookEntry entries[kMaxHookEntries];
};

struct HelperIpcCommand {
    uint32_t command;
    uint32_t threadId;
    uint64_t hwnd;
    uint32_t mainPid;
    uint32_t reserved;
};
#pragma pack(pop)

static_assert(sizeof(SharedHookEntry) == 48, "SharedHookEntry size must be identical across x86 and x64");
static_assert(sizeof(HelperIpcCommand) == 24, "HelperIpcCommand size must be identical across x86 and x64");

inline uint64_t HwndToU64(void* hwnd) noexcept {
    return static_cast<uint64_t>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(hwnd)));
}

} // namespace flexwin
