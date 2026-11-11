#include "HookBridge.h"

#include <sddl.h>
#include <vector>

#include "ConfigManager.h"
#include "Logger.h"

namespace flexwin {

namespace {

bool FileExists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

} // namespace

HookBridge::~HookBridge() {
    Shutdown();
}

bool HookBridge::Initialize(HWND mainControllerHwnd) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_msgAttach = RegisterWindowMessageW(kHookAttachMsgName);
    m_msgDetach = RegisterWindowMessageW(kHookDetachMsgName);
    m_msgUpdate = RegisterWindowMessageW(kHookUpdateMsgName);

    if (!InitSharedMemory(mainControllerHwnd)) {
        Logger::Instance().Error(L"Paylaşımlı Hook belleği oluşturulamadı.");
        return false;
    }

    const std::wstring dir = ConfigManager::GetExeDirectory();
    const std::wstring hook64Path = dir + L"\\FlexWinHook64.dll";
    const std::wstring helper32Path = dir + L"\\FlexWinHelper32.exe";
    const std::wstring hook32Path = dir + L"\\FlexWinHook32.dll";

    m_helper32ExistsOnDisk = FileExists(helper32Path);
    m_hook32ExistsOnDisk = FileExists(hook32Path);

    if (FileExists(hook64Path)) {
        m_hook64Dll = LoadLibraryW(hook64Path.c_str());
        if (m_hook64Dll) {
            m_callWndProc64 = reinterpret_cast<HOOKPROC>(
                GetProcAddress(m_hook64Dll, "FlexWinCallWndProc"));
            m_callWndRetProc64 = reinterpret_cast<HOOKPROC>(
                GetProcAddress(m_hook64Dll, "FlexWinCallWndRetProc"));
            if (!m_callWndProc64 || !m_callWndRetProc64) {
                Logger::Instance().Warn(L"FlexWinHook64.dll beklenen fonksiyonları dışa aktarmıyor.");
                FreeLibrary(m_hook64Dll);
                m_hook64Dll = nullptr;
            }
        } else {
            Logger::Instance().Warn(L"FlexWinHook64.dll yüklenemedi (hata: " +
                                    std::to_wstring(GetLastError()) + L").");
        }
    } else {
        Logger::Instance().Warn(L"FlexWinHook64.dll bulunamadı; 64-bit hook devre dışı.");
    }

    if (!m_helper32ExistsOnDisk || !m_hook32ExistsOnDisk) {
        Logger::Instance().Info(L"32-bit yardımcı bileşenler bulunamadı; yalnızca 64-bit ve dış müdahale aktif.");
    }

    return true;
}

bool HookBridge::InitSharedMemory(HWND mainControllerHwnd) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    PSECURITY_DESCRIPTOR pSD = nullptr;
    // Owner: full access, Authenticated Users: read-only, Low integrity: no write-up
    const wchar_t* sddl = L"D:(A;;GA;;;CO)(A;;GA;;;BA)(A;;GA;;;SY)(A;;GR;;;AU)S:(ML;;NW;;;LW)";
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &pSD, nullptr)) {
        sa.lpSecurityDescriptor = pSD;
    }

    const std::wstring mapName = std::wstring(kSharedTablePrefix) +
                                 std::to_wstring(GetCurrentProcessId());
    m_hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, pSD ? &sa : nullptr, PAGE_READWRITE,
                                0, sizeof(SharedHookTable), mapName.c_str());
    if (pSD) {
        LocalFree(pSD);
        pSD = nullptr;
    }
    if (!m_hMap) {
        // Fallback to default security descriptor if SDDL restricted
        m_hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                    0, sizeof(SharedHookTable), mapName.c_str());
    }
    if (!m_hMap) return false;

    m_table = static_cast<SharedHookTable*>(
        MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedHookTable)));
    if (!m_table) {
        CloseHandle(m_hMap);
        m_hMap = nullptr;
        return false;
    }

    ZeroMemory(m_table, sizeof(SharedHookTable));
    m_table->magic = kHookTableMagic;
    m_table->version = kHookTableVersion;
    m_table->mainPid = GetCurrentProcessId();
    m_table->mainHwnd = HwndToU64(mainControllerHwnd);
    return true;
}

void HookBridge::Shutdown() {
    DetachAll();

    std::lock_guard<std::mutex> lock(m_mutex);
    HWND helperWnd = FindHelper32Window();
    if (helperWnd) {
        HelperIpcCommand cmd{kHelperCmdShutdown, 0, 0, GetCurrentProcessId(), 0};
        COPYDATASTRUCT cds{0, sizeof(cmd), &cmd};
        DWORD_PTR res = 0;
        SendMessageTimeoutW(helperWnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds),
                            SMTO_ABORTIFHUNG | SMTO_NORMAL, 500, &res);
    }
    if (m_helper32Proc) {
        WaitForSingleObject(m_helper32Proc, 500);
        CloseHandle(m_helper32Proc);
        m_helper32Proc = nullptr;
    }
    if (m_hook64Dll) {
        FreeLibrary(m_hook64Dll);
        m_hook64Dll = nullptr;
        m_callWndProc64 = nullptr;
        m_callWndRetProc64 = nullptr;
    }
    if (m_table) {
        m_table->mainHwnd = 0;
        UnmapViewOfFile(m_table);
        m_table = nullptr;
    }
    if (m_hMap) {
        CloseHandle(m_hMap);
        m_hMap = nullptr;
    }
}

HWND HookBridge::FindHelper32Window() const {
    wchar_t titleBuf[64]{};
    swprintf_s(titleBuf, L"FlexWinHelper32_%lu", static_cast<unsigned long>(GetCurrentProcessId()));
    return FindWindowW(kHelper32WindowClass, titleBuf);
}

bool HookBridge::EnsureHelper32Running() {
    if (!m_helper32ExistsOnDisk || !m_hook32ExistsOnDisk) return false;

    if (FindHelper32Window() != nullptr) return true;

    if (m_helper32Proc) {
        if (WaitForSingleObject(m_helper32Proc, 0) == WAIT_TIMEOUT) {
            // Still starting up, poll briefly
            for (int i = 0; i < 20; ++i) {
                if (FindHelper32Window() != nullptr) return true;
                Sleep(25);
            }
            return FindHelper32Window() != nullptr;
        }
        CloseHandle(m_helper32Proc);
        m_helper32Proc = nullptr;
    }

    const std::wstring helperPath = ConfigManager::GetExeDirectory() + L"\\FlexWinHelper32.exe";
    std::wstring cmdLine = L"\"" + helperPath + L"\" --parent-pid " + std::to_wstring(GetCurrentProcessId());

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(helperPath.c_str(), &cmdLine[0], nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, ConfigManager::GetExeDirectory().c_str(), &si, &pi)) {
        Logger::Instance().Warn(L"FlexWinHelper32.exe başlatılamadı (hata: " +
                                std::to_wstring(GetLastError()) + L").");
        return false;
    }
    CloseHandle(pi.hThread);
    m_helper32Proc = pi.hProcess;

    for (int i = 0; i < 40; ++i) {
        if (FindHelper32Window() != nullptr) return true;
        if (WaitForSingleObject(m_helper32Proc, 0) == WAIT_OBJECT_0) break;
        Sleep(25);
    }
    return FindHelper32Window() != nullptr;
}

int HookBridge::AllocateOrFindSlot(uint64_t hwndU64) {
    if (!m_table) return -1;
    int firstFree = -1;
    for (uint32_t i = 0; i < kMaxHookEntries; ++i) {
        if (m_table->entries[i].active == 1 && m_table->entries[i].hwnd == hwndU64) {
            return static_cast<int>(i);
        }
        if (firstFree < 0 && m_table->entries[i].active == 0) {
            firstFree = static_cast<int>(i);
        }
    }
    return firstFree;
}

void HookBridge::WriteSlot(int slotIdx, uint64_t hwndU64, bool active, const HookParams& params) {
    if (!m_table || slotIdx < 0 || slotIdx >= static_cast<int>(kMaxHookEntries)) return;
    SharedHookEntry& slot = m_table->entries[slotIdx];

    InterlockedIncrement(reinterpret_cast<volatile LONG*>(&slot.seq)); // Now odd
    MemoryBarrier();
    slot.active = active ? 1u : 0u;
    slot.hwnd = active ? hwndU64 : 0ull;
    slot.flags = params.flags;
    slot.minWidth = params.minWidth;
    slot.minHeight = params.minHeight;
    slot.maxWidth = params.maxWidth;
    slot.maxHeight = params.maxHeight;
    slot.targetWidth = params.targetWidth;
    slot.targetHeight = params.targetHeight;
    MemoryBarrier();
    InterlockedIncrement(reinterpret_cast<volatile LONG*>(&slot.seq)); // Now even
}

void HookBridge::SendHookMessage(HWND hwnd, UINT msg) {
    if (msg != 0 && IsWindow(hwnd)) {
        DWORD_PTR res = 0;
        SendMessageTimeoutW(hwnd, msg, static_cast<WPARAM>(GetCurrentProcessId()), 0,
                            SMTO_ABORTIFHUNG | SMTO_NORMAL, 250, &res);
    }
}

bool HookBridge::AttachOrUpdate(HWND hwnd, DWORD threadId, bool is32Bit, const HookParams& params) {
    if (!hwnd || !IsWindow(hwnd) || threadId == 0) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_table) return false;

    const uint64_t key = HwndToU64(hwnd);
    int slotIdx = AllocateOrFindSlot(key);
    if (slotIdx < 0) {
        Logger::Instance().Warn(L"Hook tablosu dolu (maks. 256 pencere).");
        return false;
    }

    WriteSlot(slotIdx, key, true, params);

    auto existing = m_attached.find(key);
    if (existing != m_attached.end()) {
        existing->second.slotIndex = slotIdx;
        SendHookMessage(hwnd, m_msgUpdate);
        return true;
    }

    AttachedRecord rec{};
    rec.is32Bit = is32Bit;
    rec.slotIndex = slotIdx;

    if (!is32Bit) {
        if (!m_hook64Dll || !m_callWndProc64 || !m_callWndRetProc64) {
            WriteSlot(slotIdx, key, false, HookParams{});
            return false;
        }
        rec.callWnd = SetWindowsHookExW(WH_CALLWNDPROC, m_callWndProc64, m_hook64Dll, threadId);
        rec.callWndRet = SetWindowsHookExW(WH_CALLWNDPROCRET, m_callWndRetProc64, m_hook64Dll, threadId);
        if (!rec.callWnd && !rec.callWndRet) {
            WriteSlot(slotIdx, key, false, HookParams{});
            return false;
        }
        m_attached[key] = rec;
        SendHookMessage(hwnd, m_msgAttach);
        return true;
    } else {
        if (!EnsureHelper32Running()) {
            WriteSlot(slotIdx, key, false, HookParams{});
            return false;
        }
        HWND helperWnd = FindHelper32Window();
        if (!helperWnd) {
            WriteSlot(slotIdx, key, false, HookParams{});
            return false;
        }
        HelperIpcCommand cmd{kHelperCmdAttach, threadId, key, GetCurrentProcessId(), 0};
        COPYDATASTRUCT cds{0, sizeof(cmd), &cmd};
        DWORD_PTR res = 0;
        if (!SendMessageTimeoutW(helperWnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds),
                                 SMTO_ABORTIFHUNG | SMTO_NORMAL, 500, &res) || res == 0) {
            WriteSlot(slotIdx, key, false, HookParams{});
            return false;
        }
        m_attached[key] = rec;
        return true;
    }
}

void HookBridge::Detach(HWND hwnd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const uint64_t key = HwndToU64(hwnd);
    auto it = m_attached.find(key);
    if (it == m_attached.end()) return;

    const AttachedRecord rec = it->second;
    if (rec.slotIndex >= 0) {
        WriteSlot(rec.slotIndex, key, false, HookParams{});
    }

    if (!rec.is32Bit) {
        if (IsWindow(hwnd)) {
            SendHookMessage(hwnd, m_msgDetach);
        }
        if (rec.callWnd) UnhookWindowsHookEx(rec.callWnd);
        if (rec.callWndRet) UnhookWindowsHookEx(rec.callWndRet);
    } else {
        HWND helperWnd = FindHelper32Window();
        if (helperWnd) {
            HelperIpcCommand cmd{kHelperCmdDetach, 0, key, GetCurrentProcessId(), 0};
            COPYDATASTRUCT cds{0, sizeof(cmd), &cmd};
            DWORD_PTR res = 0;
            SendMessageTimeoutW(helperWnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds),
                                SMTO_ABORTIFHUNG | SMTO_NORMAL, 500, &res);
        }
    }
    m_attached.erase(it);
}

void HookBridge::DetachAll() {
    std::vector<HWND> list;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        list.reserve(m_attached.size());
        for (const auto& kv : m_attached) {
            list.push_back(reinterpret_cast<HWND>(static_cast<uintptr_t>(kv.first)));
        }
    }
    for (HWND h : list) {
        Detach(h);
    }
}

void HookBridge::ReapDeadWindows() {
    std::vector<HWND> dead;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& kv : m_attached) {
            HWND h = reinterpret_cast<HWND>(static_cast<uintptr_t>(kv.first));
            if (!IsWindow(h)) {
                dead.push_back(h);
            }
        }
    }
    for (HWND h : dead) {
        Detach(h);
    }
}

bool HookBridge::IsHook64Ready() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hook64Dll != nullptr && m_callWndProc64 != nullptr && m_callWndRetProc64 != nullptr;
}

bool HookBridge::IsHook32Available() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_helper32ExistsOnDisk && m_hook32ExistsOnDisk;
}

bool HookBridge::IsHooked(HWND hwnd) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_attached.find(HwndToU64(hwnd)) != m_attached.end();
}

size_t HookBridge::ActiveHookCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_attached.size();
}

} // namespace flexwin
