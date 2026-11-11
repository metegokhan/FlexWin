#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <CommCtrl.h>
#include <shellapi.h>
#include <cstring>
#include <string>

#include "core/ConfigManager.h"
#include "core/I18n.h"
#include "core/Logger.h"
#include "ui/AppWindow.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    bool startMinimized = (nCmdShow == SW_SHOWMINIMIZED || nCmdShow == SW_MINIMIZE || nCmdShow == SW_HIDE);
    bool elevatedRestart = false;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (wcscmp(argv[i], L"--minimized") == 0) startMinimized = true;
            else if (wcscmp(argv[i], L"--elevated-restart") == 0) elevatedRestart = true;
        }
        LocalFree(argv);
    }

    HANDLE singleMutex = CreateMutexW(nullptr, TRUE, L"Local\\FlexWin_SingleInstance_v1");
    if (singleMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (elevatedRestart) {
            DWORD waitRes = WaitForSingleObject(singleMutex, 3000);
            if (waitRes != WAIT_OBJECT_0 && waitRes != WAIT_ABANDONED) {
                CloseHandle(singleMutex);
                return 0;
            }
        } else {
            UINT showMsg = RegisterWindowMessageW(L"FlexWin_ShowMainUI");
            HWND existing = FindWindowW(L"FlexWinMainWindowClass", nullptr);
            if (existing && showMsg != 0) {
                PostMessageW(existing, showMsg, 0, 0);
            }
            CloseHandle(singleMutex);
            return 0;
        }
    }

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    const std::wstring configPath = flexwin::ConfigManager::GetDefaultConfigPath();
    flexwin::AppConfig config;
    std::wstring loadErr;
    if (!flexwin::ConfigManager::Load(configPath, config, &loadErr)) {
        config = flexwin::ConfigManager::CreateDefaultConfig();
        flexwin::I18n::SetLanguageCode(config.language);
        flexwin::ConfigManager::Save(configPath, config);
        flexwin::Logger::Instance().Info(L"İlk çalışma: varsayılan rules.json oluşturuldu.");
    } else {
        flexwin::I18n::SetLanguageCode(config.language);
        flexwin::Logger::Instance().Info(
            flexwin::I18n::GetLanguage() == flexwin::Language::En
                ? L"rules.json loaded successfully."
                : L"rules.json başarıyla yüklendi.");
    }

    if (!flexwin::AppWindow::Register(hInstance)) {
        if (singleMutex) {
            ReleaseMutex(singleMutex);
            CloseHandle(singleMutex);
        }
        return 1;
    }

    flexwin::AppWindow mainWin;
    if (!mainWin.Create(hInstance, std::move(config), configPath, startMinimized)) {
        if (singleMutex) {
            ReleaseMutex(singleMutex);
            CloseHandle(singleMutex);
        }
        return 2;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(mainWin.GetHwnd(), &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    mainWin.Shutdown();

    if (singleMutex) {
        ReleaseMutex(singleMutex);
        CloseHandle(singleMutex);
    }
    return static_cast<int>(msg.wParam);
}
