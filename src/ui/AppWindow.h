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

#include "../core/HotkeyManager.h"
#include "../core/Models.h"
#include "../core/MonitorManager.h"
#include "../core/WindowEngine.h"
#include "CrosshairPicker.h"
#include "LogDialog.h"
#include "MonitorPreview.h"
#include "OpenWindowsDialog.h"
#include "TrayIcon.h"

namespace flexwin {

class AppWindow {
public:
    static bool Register(HINSTANCE hInst);

    bool Create(HINSTANCE hInst, AppConfig config, const std::wstring& configPath, bool startMinimized);
    void Shutdown();
    HWND GetHwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateControls();
    void CreateFontsForDpi(UINT dpi);
    void ApplyFontsToChildren();
    void LayoutControls(int cw, int ch);

    void RefreshMonitorList();
    void RefreshProfileCombo();
    void RefreshRulesList(int selectIdx = -1);
    void LoadRuleIntoEditor(int idx);
    void ReadEditorIntoCurrentRule();
    void SetEditorEnabled(bool enabled);

    void UpdateGeometryLabels();
    void UpdatePreview();
    void UpdateStatusBar();
    void UpdateMonitorToggleButton();
    void ApplyLanguage();
    void RepopulateTranslatedCombos();

    void ApplyPreset(double xp, double yp, double wp, double hp);
    void PopulateRuleFromWindow(const WindowInfo& info, bool createNewRule);
    void SaveConfig(bool reapplyEngine = true);
    void ExportActiveProfileDialog();
    void ImportProfilesDialog();

    void HandleTrayCommand(UINT cmdId);
    void HandleHotkey(int hotkeyId);
    void ShowOrRestoreWindow();

    Profile* GetActiveProfile();
    WindowRule* GetSelectedRule();

    HINSTANCE m_hInst = nullptr;
    HWND m_hwnd = nullptr;
    UINT m_dpi = 96;
    HFONT m_font = nullptr;
    HFONT m_fontBold = nullptr;
    HICON m_appIcon = nullptr;

    std::wstring m_configPath;
    AppConfig m_config;
    WindowEngine m_engine;
    HotkeyManager m_hotkeys;
    TrayIcon m_tray;
    OpenWindowsDialog m_openWinsDlg;
    LogDialog m_logDlg;

    std::vector<MonitorInfoEx> m_monitors;
    int m_selectedRuleIndex = -1;
    bool m_populatingUi = false;
    bool m_exiting = false;
    bool m_animTimerRunning = false;
    HWND m_lastTestedHwnd = nullptr;

    // Top bar controls
    HWND m_lblProfile = nullptr;
    HWND m_comboProfile = nullptr;
    HWND m_btnNewProfile = nullptr;
    HWND m_btnDelProfile = nullptr;
    HWND m_btnExportProfile = nullptr;
    HWND m_btnImportProfile = nullptr;
    HWND m_btnToggleMonitor = nullptr;
    HWND m_btnApplyAll = nullptr;
    HWND m_btnOpenWindows = nullptr;
    HWND m_btnLogs = nullptr;
    HWND m_btnLang = nullptr;
    HWND m_btnAdmin = nullptr;

    // Left panel
    HWND m_grpRules = nullptr;
    HWND m_listRules = nullptr;
    HWND m_btnAddRule = nullptr;
    HWND m_btnDupRule = nullptr;
    HWND m_btnDelRule = nullptr;
    HWND m_btnMoveUp = nullptr;
    HWND m_btnMoveDown = nullptr;
    HWND m_lblHotkeyHint = nullptr;

    // Right panel - Section 1: Target & Matching
    HWND m_grpMatch = nullptr;
    CrosshairPicker m_picker;
    HWND m_btnPickFromOpen = nullptr;
    HWND m_lblPickedInfo = nullptr;
    HWND m_lblRuleName = nullptr;
    HWND m_editRuleName = nullptr;
    HWND m_chkEnabled = nullptr;
    HWND m_chkProcess = nullptr;
    HWND m_editProcess = nullptr;
    HWND m_chkClass = nullptr;
    HWND m_editClass = nullptr;
    HWND m_lblTitle = nullptr;
    HWND m_comboTitleMode = nullptr;
    HWND m_editTitle = nullptr;
    HWND m_chkExcludePopups = nullptr;
    HWND m_lblMinSrc = nullptr;
    HWND m_editMinSrcW = nullptr;
    HWND m_editMinSrcH = nullptr;

    // Right panel - Section 2: Action, Geometry & Monitor Preview
    HWND m_grpGeom = nullptr;
    HWND m_lblWindowAction = nullptr;
    HWND m_comboWindowAction = nullptr;
    HWND m_lblActionDelay = nullptr;
    HWND m_editActionDelay = nullptr;
    HWND m_lblGeomMode = nullptr;
    HWND m_comboGeomMode = nullptr;
    HWND m_lblMonitor = nullptr;
    HWND m_comboMonitor = nullptr;
    HWND m_lblX = nullptr;
    HWND m_editX = nullptr;
    HWND m_lblY = nullptr;
    HWND m_editY = nullptr;
    HWND m_lblW = nullptr;
    HWND m_editW = nullptr;
    HWND m_lblH = nullptr;
    HWND m_editH = nullptr;
    HWND m_btnPresetLeft = nullptr;
    HWND m_btnPresetRight = nullptr;
    HWND m_btnPresetTop = nullptr;
    HWND m_btnPresetBottom = nullptr;
    HWND m_btnPresetFull = nullptr;
    HWND m_btnPresetTL = nullptr;
    HWND m_btnPresetTR = nullptr;
    HWND m_btnPresetBL = nullptr;
    HWND m_btnPresetBR = nullptr;
    HWND m_btnPresetCenter = nullptr;
    MonitorPreview m_preview;

    // Right panel - Section 3: Enforcement, Opacity & Options
    HWND m_grpOpts = nullptr;
    HWND m_chkResizable = nullptr;
    HWND m_chkBorderless = nullptr;
    HWND m_chkTopMost = nullptr;
    HWND m_chkDwmMargin = nullptr;
    HWND m_chkAggressive = nullptr;
    HWND m_chkHookDll = nullptr;
    HWND m_chkHitTest = nullptr;
    HWND m_chkLockSize = nullptr;
    HWND m_chkScaleChildren = nullptr;
    HWND m_chkCustomMinMax = nullptr;
    HWND m_editMinW = nullptr;
    HWND m_editMinH = nullptr;
    HWND m_editMaxW = nullptr;
    HWND m_editMaxH = nullptr;
    HWND m_chkOpacity = nullptr;
    HWND m_sliderOpacity = nullptr;
    HWND m_lblOpacityVal = nullptr;
    HWND m_chkSmoothFade = nullptr;

    // Action row & StatusBar
    HWND m_btnTestRule = nullptr;
    HWND m_btnRevertLast = nullptr;
    HWND m_btnSave = nullptr;
    HWND m_statusBar = nullptr;
};

} // namespace flexwin
