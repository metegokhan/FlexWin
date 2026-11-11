#include "I18n.h"

#include <atomic>
#include "StringUtil.h"

namespace flexwin {

namespace {
std::atomic<int> g_lang{static_cast<int>(Language::Tr)};

struct Bilingual {
    const wchar_t* tr;
    const wchar_t* en;
};

Bilingual Lookup(StrKey key) {
    switch (key) {
    case StrKey::AppTitle:              return {L"FlexWin — Hafif Kurumsal Pencere Yöneticisi", L"FlexWin — Lightweight Enterprise Window Manager"};
    case StrKey::AppTitleAdmin:         return {L"FlexWin — Hafif Kurumsal Pencere Yöneticisi [Yönetici]", L"FlexWin — Lightweight Enterprise Window Manager [Admin]"};
    case StrKey::TrayTooltip:           return {L"FlexWin — Kurumsal Pencere Yöneticisi", L"FlexWin — Enterprise Window Manager"};
    case StrKey::LangToggleBtn:         return {L"TR | EN", L"EN | TR"};

    case StrKey::LblProfile:            return {L"Profil:", L"Profile:"};
    case StrKey::BtnNewProfile:         return {L"+ Profil", L"+ Profile"};
    case StrKey::BtnDelProfile:         return {L"Sil", L"Del"};
    case StrKey::BtnExportProfile:      return {L"Dışa Aktar", L"Export"};
    case StrKey::BtnImportProfile:      return {L"İçe Aktar", L"Import"};
    case StrKey::BtnMonitorOn:          return {L"İzleme: Açık", L"Watch: On"};
    case StrKey::BtnMonitorOff:         return {L"İzleme: Duraklatıldı", L"Watch: Paused"};
    case StrKey::BtnApplyAll:           return {L"Tümünü Uygula", L"Apply All"};
    case StrKey::BtnOpenWindows:        return {L"Açık Pencereler", L"Open Windows"};
    case StrKey::BtnLogs:               return {L"Günlük", L"Logs"};
    case StrKey::BtnAdmin:              return {L"Yönetici Ol", L"Run as Admin"};
    case StrKey::BtnAdminActive:        return {L"Yönetici Modu", L"Admin Mode"};

    case StrKey::GrpRules:              return {L"Kurallar (Öncelik Sırası)", L"Rules (Priority Order)"};
    case StrKey::ColRule:               return {L"Kural", L"Rule"};
    case StrKey::ColTarget:             return {L"Hedef (.exe)", L"Target (.exe)"};
    case StrKey::ColModeAction:         return {L"Alan / İşlem", L"Area / Action"};
    case StrKey::BtnAddRule:            return {L"+ Ekle", L"+ Add"};
    case StrKey::BtnDupRule:            return {L"Kopyala", L"Copy"};
    case StrKey::BtnDelRule:            return {L"Sil", L"Delete"};
    case StrKey::BtnMoveUp:             return {L"Yukarı", L"Up"};
    case StrKey::BtnMoveDown:           return {L"Aşağı", L"Down"};
    case StrKey::LblHotkeyHint:         return {L"Kısayollar: Ctrl+Alt+R (Kilit Aç) | B (Borderless) | T (Üstte) | A (Tümünü Uygula)", L"Hotkeys: Ctrl+Alt+R (Unlock) | B (Borderless) | T (TopMost) | A (Apply All)"};

    case StrKey::GrpMatch:              return {L"1. Hedef Pencere Yakalama ve Eşleştirme", L"1. Target Window Picker & Matching"};
    case StrKey::PickerReady:           return {L"Hedef Pencereye Sürükle & Bırak (Nişangah)", L"Drag & Drop onto Target Window (Crosshair)"};
    case StrKey::PickerDragging:        return {L"Hedef pencere üzerine bırakın (Esc: İptal)...", L"Drop over target window (Esc: Cancel)..."};
    case StrKey::BtnPickOpen:           return {L"Listeden Seç...", L"Pick from List..."};
    case StrKey::PickedInfoDefault:     return {L"Nişangahı hedef kurumsal pencere üzerine sürükleyip bırakın.", L"Drag and drop the crosshair over any target window."};
    case StrKey::PickedProtected:       return {L"Sistem penceresi korumalıdır; seçilemez.", L"System window is protected and cannot be picked."};
    case StrKey::PickedCancelled:       return {L"Seçim iptal edildi veya pencere seçilmedi.", L"Selection cancelled or no window picked."};
    case StrKey::LblRuleName:           return {L"Kural Adı:", L"Rule Name:"};
    case StrKey::ChkEnabled:            return {L"Kural Etkin", L"Rule Enabled"};
    case StrKey::ChkProcess:            return {L"Süreç (.exe):", L"Process (.exe):"};
    case StrKey::ChkClass:              return {L"Sınıf (Class):", L"Class Name:"};
    case StrKey::LblTitle:              return {L"Başlık:", L"Title:"};
    case StrKey::TitleIgnore:           return {L"Yoksay", L"Ignore"};
    case StrKey::TitleContains:         return {L"İçerir", L"Contains"};
    case StrKey::TitleExact:            return {L"Tam Eşleşme", L"Exact"};
    case StrKey::TitleStartsWith:       return {L"İle Başlar", L"Starts With"};
    case StrKey::TitleRegex:            return {L"Regex", L"Regex"};
    case StrKey::ChkExcludePopups:      return {L"Alt pop-up ve sistem diyaloglarını (#32770) koru", L"Exclude child popups and system dialogs (#32770)"};
    case StrKey::LblMinSrc:             return {L"Min. Kaynak (G/Y):", L"Min Source (W/H):"};

    case StrKey::GrpGeom:               return {L"2. Hedef Konum, Boyut, Eylem ve Monitör Alanı", L"2. Target Geometry, Lifecycle Action & Monitor"};
    case StrKey::LblWindowAction:       return {L"Pencere Eylemi:", L"Window Action:"};
    case StrKey::ActNormal:             return {L"Boyutlandır / Konumlandır / Stil", L"Resize / Position / Style"};
    case StrKey::ActMinimize:           return {L"Otomatik Küçült (Minimize)", L"Auto Minimize"};
    case StrKey::ActClose:              return {L"Otomatik Kapat (Close)", L"Auto Close"};
    case StrKey::LblDelayMs:            return {L"Gecikme (ms):", L"Delay (ms):"};
    case StrKey::LblGeomMode:           return {L"Yerleşim Modu:", L"Layout Mode:"};
    case StrKey::GeomPixels:            return {L"Sabit Piksel (X, Y, G, Y)", L"Fixed Pixels (X, Y, W, H)"};
    case StrKey::GeomPercent:           return {L"Monitör Yüzdesi (%)", L"Monitor Percent (%)"};
    case StrKey::GeomSizeOnly:          return {L"Sadece Boyutlandır (Piksel)", L"Size Only (Pixels)"};
    case StrKey::GeomPosOnly:           return {L"Sadece Konumlandır (Piksel)", L"Position Only (Pixels)"};
    case StrKey::GeomStyleOnly:         return {L"Yalnızca Resize Kilidini Aç", L"Unlock Style Only"};
    case StrKey::LblMonitor:            return {L"Hedef Monitör:", L"Target Monitor:"};
    case StrKey::MonWindowCurrent:      return {L"Pencerenin Açıldığı Monitör", L"Window's Current Monitor"};
    case StrKey::MonPrimary:            return {L"Birincil Monitör", L"Primary Monitor"};
    case StrKey::MonIndexFmt:           return {L"Monitör #%zu (%ldx%ld%s)", L"Monitor #%zu (%ldx%ld%s)"};
    case StrKey::MonPrimarySuffix:      return {L" - Birincil", L" - Primary"};
    case StrKey::LblXPct:               return {L"X (%):", L"X (%):"};
    case StrKey::LblYPct:               return {L"Y (%):", L"Y (%):"};
    case StrKey::LblWPct:               return {L"Gen (%):", L"W (%):"};
    case StrKey::LblHPct:               return {L"Yük (%):", L"H (%):"};
    case StrKey::LblXPx:                return {L"X (px):", L"X (px):"};
    case StrKey::LblYPx:                return {L"Y (px):", L"Y (px):"};
    case StrKey::LblWPx:                return {L"Gen (px):", L"W (px):"};
    case StrKey::LblHPx:                return {L"Yük (px):", L"H (px):"};
    case StrKey::PresetLeft:            return {L"Sol ½", L"Left ½"};
    case StrKey::PresetRight:           return {L"Sağ ½", L"Right ½"};
    case StrKey::PresetTop:             return {L"Üst ½", L"Top ½"};
    case StrKey::PresetBottom:          return {L"Alt ½", L"Bottom ½"};
    case StrKey::PresetFull:            return {L"Tam Alan", L"Full Work"};
    case StrKey::PresetTL:              return {L"Sol-Üst ¼", L"Top-Left ¼"};
    case StrKey::PresetTR:              return {L"Sağ-Üst ¼", L"Top-Right ¼"};
    case StrKey::PresetBL:              return {L"Sol-Alt ¼", L"Bot-Left ¼"};
    case StrKey::PresetBR:              return {L"Sağ-Alt ¼", L"Bot-Right ¼"};
    case StrKey::PresetCenter:          return {L"Orta %80", L"Center 80%"};
    case StrKey::PreviewStyleOnly:      return {L"Yalnızca Stil Modu (Konum/Boyut değişmez)", L"Style Only Mode (Geometry unchanged)"};
    case StrKey::PreviewMinimize:       return {L"Otomatik Küçült (Minimize) Modu", L"Auto Minimize Mode"};
    case StrKey::PreviewClose:          return {L"Otomatik Kapat (Close) Modu", L"Auto Close Mode"};

    case StrKey::GrpOpts:               return {L"3. Zorlama, Hook DLL, Şeffaflık ve İleri Seçenekler", L"3. Force Mode, Hook DLL, Opacity & Options"};
    case StrKey::ChkResizable:          return {L"Resize Kenarlarını Aç (WS_THICKFRAME)", L"Unlock Resize Borders (WS_THICKFRAME)"};
    case StrKey::ChkBorderless:         return {L"Başlık Çubuğunu Kaldır (Borderless)", L"Remove Title Bar (Borderless)"};
    case StrKey::ChkTopMost:            return {L"Her Zaman Üstte Tut (Always on Top)", L"Always on Top"};
    case StrKey::ChkDwmMargin:          return {L"Görünmez Kenar Boşluğunu Telafi Et (DWM)", L"Compensate Invisible DWM Margins"};
    case StrKey::ChkAggressive:         return {L"Agresif Zorlama + Geri Zıplama Koruması", L"Aggressive Force + Anti-Snapback"};
    case StrKey::ChkHookDll:            return {L"Hook DLL Kullan (WM_GETMINMAXINFO Kırıcı)", L"Use Hook DLL (Break WM_GETMINMAXINFO)"};
    case StrKey::ChkHitTest:            return {L"Özel Kenar Tutma (WM_NCHITTEST Subclass)", L"Custom Edge Grab (WM_NCHITTEST Subclass)"};
    case StrKey::ChkLockSize:           return {L"Hedef Boyutu Kilitle (Değiştirilmesin)", L"Lock Target Size (Prevent Resize)"};
    case StrKey::ChkScaleChildren:      return {L"Deneysel: Win32 Alt Kontrolleri Ölçekle", L"Experimental: Scale Win32 Child Controls"};
    case StrKey::ChkCustomMinMax:       return {L"Özel Min/Max (G, Y):", L"Custom Min/Max (W, H):"};
    case StrKey::ChkOpacity:            return {L"Dereceli Şeffaflık:", L"Opacity (%):"};
    case StrKey::ChkSmoothFade:         return {L"Yumuşak Geçiş", L"Smooth Fade"};

    case StrKey::BtnTestRule:           return {L"Bu Kuralı Şimdi Test Et / Uygula", L"Test / Apply This Rule Now"};
    case StrKey::BtnRevertLast:         return {L"Geri Al (Eski Boyut)", L"Revert Last Window"};
    case StrKey::BtnSave:               return {L"Kaydet ve Etkinleştir", L"Save & Apply"};

    case StrKey::SbProfileFmt:          return {L"Profil: %s (%zu/%zu kural)", L"Profile: %s (%zu/%zu rules)"};
    case StrKey::SbHookFmt:             return {L"Hook x64: %s | x86: %s | Aktif: %zu", L"Hook x64: %s | x86: %s | Active: %zu"};
    case StrKey::SbReady:               return {L"Hazır — Kurumsal pencere kuralları bekleniyor.", L"Ready — Waiting for enterprise windows."};
    case StrKey::SbHookReady:           return {L"Hazır", L"Ready"};
    case StrKey::SbHookMissing:         return {L"Yok", L"Missing"};

    case StrKey::PromptNewProfileTitle: return {L"Yeni Profil Oluştur", L"Create New Profile"};
    case StrKey::PromptNewProfileLabel: return {L"Profil adı:", L"Profile name:"};
    case StrKey::BtnOk:                 return {L"Tamam", L"OK"};
    case StrKey::BtnCancel:             return {L"İptal", L"Cancel"};
    case StrKey::DefaultNewProfileName: return {L"Yeni Kurumsal Profil", L"New Enterprise Profile"};
    case StrKey::DefaultNewRuleName:    return {L"Yeni Kural", L"New Rule"};
    case StrKey::CapturedRuleFallback:  return {L"Yakalanan Pencere", L"Captured Window"};
    case StrKey::CopySuffix:            return {L" (Kopya)", L" (Copy)"};

    case StrKey::ExportDialogTitle:     return {L"Profili Dışa Aktar (Yedekle)", L"Export Profile Backup"};
    case StrKey::ImportDialogTitle:     return {L"Profili İçe Aktar", L"Import Profile Backup"};
    case StrKey::ExportSuccessMsg:      return {L"Profil yedeği başarıyla kaydedildi.", L"Profile backup exported successfully."};
    case StrKey::ImportSuccessFmt:      return {L"%zu profil başarıyla içe aktarıldı.", L"Successfully imported %zu profile(s)."};
    case StrKey::ExportErrorTitle:      return {L"Dışa Aktarma Hatası", L"Export Error"};
    case StrKey::ImportErrorTitle:      return {L"İçe Aktarma Hatası", L"Import Error"};
    case StrKey::SaveErrorTitle:        return {L"FlexWin Kayıt Hatası", L"FlexWin Save Error"};

    case StrKey::TestNoMatchTitle:      return {L"FlexWin Test", L"FlexWin Test"};
    case StrKey::TestNoMatchBody:       return {L"Bu kurala uyan açık bir pencere bulunamadı.\nHedef uygulamanın açık olduğundan ve Süreç/Sınıf/Başlık filtrelerinin doğru olduğundan emin olun.", L"No open window matched this rule.\nVerify the target application is open and Process/Class/Title filters match."};
    case StrKey::RevertNotFound:        return {L"Geri alınacak son test penceresi bulunamadı.", L"No recent tested window to revert."};
    case StrKey::ConfigSavedLog:        return {L"Yapılandırma rules.json dosyasına kaydedildi.", L"Configuration saved to rules.json."};
    case StrKey::AppliedAllFmt:         return {L"Tüm kurallar uygulandı (etkilenen pencere: %d).", L"Applied all rules (affected windows: %d)."};

    case StrKey::TrayOpen:              return {L"FlexWin Penceresini Aç", L"Open FlexWin"};
    case StrKey::TrayApplyAll:          return {L"Tüm Kuralları Şimdi Uygula", L"Apply All Rules Now"};
    case StrKey::TrayMonitoring:        return {L"Otomatik Pencere İzleme", L"Automatic Window Watch"};
    case StrKey::TrayProfiles:          return {L"Aktif Profil", L"Active Profile"};
    case StrKey::TrayExport:            return {L"Aktif Profili Dışa Aktar...", L"Export Active Profile..."};
    case StrKey::TrayImport:            return {L"Profil İçe Aktar...", L"Import Profile..."};
    case StrKey::TrayLanguage:          return {L"Dil / Language", L"Language / Dil"};
    case StrKey::TrayLangTr:            return {L"Türkçe (TR)", L"Turkish (TR)"};
    case StrKey::TrayLangEn:            return {L"İngilizce (EN)", L"English (EN)"};
    case StrKey::TraySettings:          return {L"Ayarlar", L"Settings"};
    case StrKey::TrayHookEngine:        return {L"Hook Motoru (WM_GETMINMAXINFO Kırıcı)", L"Hook Engine (WM_GETMINMAXINFO Unlocker)"};
    case StrKey::TrayStartup:           return {L"Windows Başlangıcında Çalıştır", L"Run at Windows Startup"};
    case StrKey::TrayStartMin:          return {L"Tepside Küçültülmüş Başlat", L"Start Minimized to Tray"};
    case StrKey::TrayCloseToTray:       return {L"Kapatınca Tepsiye Küçült", L"Minimize to Tray on Close"};
    case StrKey::TrayRunAdmin:          return {L"Yönetici Olarak Yeniden Başlat", L"Restart as Administrator"};
    case StrKey::TrayExit:              return {L"Çıkış", L"Exit"};

    case StrKey::OwTitle:               return {L"Açık Pencereler Tarayıcısı — Kural Oluştur veya İşlem Yap", L"Open Windows Inspector — Create Rule or Act Now"};
    case StrKey::OwColProc:             return {L"Süreç (.exe)", L"Process (.exe)"};
    case StrKey::OwColTitle:            return {L"Pencere Başlığı", L"Window Title"};
    case StrKey::OwColClass:            return {L"Sınıf (Class)", L"Class Name"};
    case StrKey::OwColSize:             return {L"Boyut", L"Size"};
    case StrKey::OwColResize:           return {L"Resize", L"Resize"};
    case StrKey::OwColArch:             return {L"Mimari / Yetki", L"Arch / Privilege"};
    case StrKey::OwResizable:           return {L"Açık", L"Unlocked"};
    case StrKey::OwLocked:              return {L"Kilitli", L"Locked"};
    case StrKey::OwUnknown:             return {L"(bilinmiyor)", L"(unknown)"};
    case StrKey::OwRefresh:             return {L"Yenile", L"Refresh"};
    case StrKey::OwCreateRule:          return {L"Seçili Pencereden Kural Oluştur", L"Create Rule from Selected"};
    case StrKey::OwQuickUnlock:         return {L"Resize Kilidini Aç", L"Unlock Resize"};
    case StrKey::OwQuickMinimize:       return {L"Küçült", L"Minimize"};
    case StrKey::OwQuickClose:          return {L"Kapat", L"Close Win"};
    case StrKey::OwClose:               return {L"Kapat", L"Close"};

    case StrKey::LogTitle:              return {L"FlexWin Olay ve Hook Günlüğü", L"FlexWin Event & Hook Log"};
    case StrKey::LogRefresh:            return {L"Yenile", L"Refresh"};
    case StrKey::LogClear:              return {L"Temizle", L"Clear"};
    case StrKey::LogClose:              return {L"Kapat", L"Close"};
    case StrKey::LogTagInfo:            return {L"[BİLGİ] ", L"[INFO]  "};
    case StrKey::LogTagOk:              return {L"[OK]    ", L"[OK]    "};
    case StrKey::LogTagWarn:            return {L"[UYARI] ", L"[WARN]  "};
    case StrKey::LogTagErr:             return {L"[HATA]  ", L"[ERR]   "};

    case StrKey::SummaryStyleOnly:      return {L"Yalnızca Stil", L"Style Only"};
    case StrKey::SummaryMinimize:       return {L"Küçült", L"Minimize"};
    case StrKey::SummaryClose:          return {L"Kapat", L"Close"};
    }
    return {L"", L""};
}

} // namespace

Language I18n::GetLanguage() {
    return static_cast<Language>(g_lang.load());
}

void I18n::SetLanguage(Language lang) {
    g_lang.store(static_cast<int>(lang));
}

void I18n::SetLanguageCode(const std::wstring& code) {
    if (EqualsIgnoreCase(code, L"en")) {
        SetLanguage(Language::En);
    } else {
        SetLanguage(Language::Tr);
    }
}

std::wstring I18n::GetLanguageCode() {
    return (GetLanguage() == Language::En) ? L"en" : L"tr";
}

const wchar_t* I18n::Get(StrKey key) {
    const Bilingual b = Lookup(key);
    return (GetLanguage() == Language::En) ? b.en : b.tr;
}

} // namespace flexwin
