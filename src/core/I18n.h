#pragma once

#include <string>

namespace flexwin {

enum class Language {
    Tr = 0,
    En = 1
};

enum class StrKey {
    AppTitle,
    AppTitleAdmin,
    TrayTooltip,
    LangToggleBtn,

    LblProfile,
    BtnNewProfile,
    BtnDelProfile,
    BtnExportProfile,
    BtnImportProfile,
    BtnMonitorOn,
    BtnMonitorOff,
    BtnApplyAll,
    BtnOpenWindows,
    BtnLogs,
    BtnAdmin,
    BtnAdminActive,

    GrpRules,
    ColRule,
    ColTarget,
    ColModeAction,
    BtnAddRule,
    BtnDupRule,
    BtnDelRule,
    BtnMoveUp,
    BtnMoveDown,
    LblHotkeyHint,

    GrpMatch,
    PickerReady,
    PickerDragging,
    BtnPickOpen,
    PickedInfoDefault,
    PickedProtected,
    PickedCancelled,
    LblRuleName,
    ChkEnabled,
    ChkProcess,
    ChkClass,
    LblTitle,
    TitleIgnore,
    TitleContains,
    TitleExact,
    TitleStartsWith,
    TitleRegex,
    ChkExcludePopups,
    LblMinSrc,

    GrpGeom,
    LblWindowAction,
    ActNormal,
    ActMinimize,
    ActClose,
    LblDelayMs,
    LblGeomMode,
    GeomPixels,
    GeomPercent,
    GeomSizeOnly,
    GeomPosOnly,
    GeomStyleOnly,
    LblMonitor,
    MonWindowCurrent,
    MonPrimary,
    MonIndexFmt,
    MonPrimarySuffix,
    LblXPct,
    LblYPct,
    LblWPct,
    LblHPct,
    LblXPx,
    LblYPx,
    LblWPx,
    LblHPx,
    PresetLeft,
    PresetRight,
    PresetTop,
    PresetBottom,
    PresetFull,
    PresetTL,
    PresetTR,
    PresetBL,
    PresetBR,
    PresetCenter,
    PreviewStyleOnly,
    PreviewMinimize,
    PreviewClose,

    GrpOpts,
    ChkResizable,
    ChkBorderless,
    ChkTopMost,
    ChkDwmMargin,
    ChkAggressive,
    ChkHookDll,
    ChkHitTest,
    ChkLockSize,
    ChkScaleChildren,
    ChkCustomMinMax,
    ChkOpacity,
    ChkSmoothFade,

    BtnTestRule,
    BtnRevertLast,
    BtnSave,

    SbProfileFmt,
    SbHookFmt,
    SbReady,
    SbHookReady,
    SbHookMissing,

    PromptNewProfileTitle,
    PromptNewProfileLabel,
    BtnOk,
    BtnCancel,
    DefaultNewProfileName,
    DefaultNewRuleName,
    CapturedRuleFallback,
    CopySuffix,

    ExportDialogTitle,
    ImportDialogTitle,
    ExportSuccessMsg,
    ImportSuccessFmt,
    ExportErrorTitle,
    ImportErrorTitle,
    SaveErrorTitle,

    TestNoMatchTitle,
    TestNoMatchBody,
    RevertNotFound,
    ConfigSavedLog,
    AppliedAllFmt,

    TrayOpen,
    TrayApplyAll,
    TrayMonitoring,
    TrayProfiles,
    TrayExport,
    TrayImport,
    TrayLanguage,
    TrayLangTr,
    TrayLangEn,
    TraySettings,
    TrayHookEngine,
    TrayStartup,
    TrayStartMin,
    TrayCloseToTray,
    TrayRunAdmin,
    TrayExit,

    OwTitle,
    OwColProc,
    OwColTitle,
    OwColClass,
    OwColSize,
    OwColResize,
    OwColArch,
    OwResizable,
    OwLocked,
    OwUnknown,
    OwRefresh,
    OwCreateRule,
    OwQuickUnlock,
    OwQuickMinimize,
    OwQuickClose,
    OwClose,

    LogTitle,
    LogRefresh,
    LogClear,
    LogClose,
    LogTagInfo,
    LogTagOk,
    LogTagWarn,
    LogTagErr,

    SummaryStyleOnly,
    SummaryMinimize,
    SummaryClose
};

class I18n {
public:
    static Language GetLanguage();
    static void SetLanguage(Language lang);
    static void SetLanguageCode(const std::wstring& code);
    static std::wstring GetLanguageCode();
    static const wchar_t* Get(StrKey key);
};

inline const wchar_t* Tr(StrKey key) {
    return I18n::Get(key);
}

} // namespace flexwin
