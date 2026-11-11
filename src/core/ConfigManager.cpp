#include "ConfigManager.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <random>

#include "../json/Json.h"
#include "StringUtil.h"

namespace flexwin {

namespace {

const char* TitleModeToStr(TitleMatchMode m) {
    switch (m) {
    case TitleMatchMode::Ignore: return "ignore";
    case TitleMatchMode::Contains: return "contains";
    case TitleMatchMode::Exact: return "exact";
    case TitleMatchMode::StartsWith: return "startsWith";
    case TitleMatchMode::Regex: return "regex";
    }
    return "ignore";
}

TitleMatchMode StrToTitleMode(const std::string& s) {
    if (s == "contains") return TitleMatchMode::Contains;
    if (s == "exact") return TitleMatchMode::Exact;
    if (s == "startsWith") return TitleMatchMode::StartsWith;
    if (s == "regex") return TitleMatchMode::Regex;
    return TitleMatchMode::Ignore;
}

const char* GeometryModeToStr(GeometryMode m) {
    switch (m) {
    case GeometryMode::Pixels: return "pixels";
    case GeometryMode::Percent: return "percent";
    case GeometryMode::SizeOnlyPixels: return "sizeOnly";
    case GeometryMode::PositionOnlyPixels: return "positionOnly";
    case GeometryMode::StyleOnly: return "styleOnly";
    }
    return "percent";
}

GeometryMode StrToGeometryMode(const std::string& s) {
    if (s == "pixels") return GeometryMode::Pixels;
    if (s == "sizeOnly") return GeometryMode::SizeOnlyPixels;
    if (s == "positionOnly") return GeometryMode::PositionOnlyPixels;
    if (s == "styleOnly") return GeometryMode::StyleOnly;
    return GeometryMode::Percent;
}

const char* MonitorModeToStr(MonitorTargetMode m) {
    switch (m) {
    case MonitorTargetMode::WindowCurrent: return "windowCurrent";
    case MonitorTargetMode::Primary: return "primary";
    case MonitorTargetMode::Index: return "index";
    }
    return "windowCurrent";
}

MonitorTargetMode StrToMonitorMode(const std::string& s) {
    if (s == "primary") return MonitorTargetMode::Primary;
    if (s == "index") return MonitorTargetMode::Index;
    return MonitorTargetMode::WindowCurrent;
}

const char* ActionToStr(WindowAction a) {
    switch (a) {
    case WindowAction::Normal: return "normal";
    case WindowAction::AutoMinimize: return "minimize";
    case WindowAction::AutoClose: return "close";
    }
    return "normal";
}

WindowAction StrToAction(const std::string& s) {
    if (s == "minimize") return WindowAction::AutoMinimize;
    if (s == "close") return WindowAction::AutoClose;
    return WindowAction::Normal;
}

json::Value HotkeyToJson(const HotkeyBinding& h) {
    auto obj = json::Value::MakeObject();
    obj.Set("enabled", h.enabled);
    obj.Set("modifiers", static_cast<int>(h.modifiers));
    obj.Set("vk", static_cast<int>(h.vk));
    return obj;
}

HotkeyBinding JsonToHotkey(const json::Value* v, const HotkeyBinding& def) {
    if (!v || !v->IsObject()) return def;
    HotkeyBinding h = def;
    h.enabled = v->GetBool("enabled", def.enabled);
    h.modifiers = static_cast<uint32_t>(v->GetInt("modifiers", static_cast<int>(def.modifiers)));
    h.vk = static_cast<uint32_t>(v->GetInt("vk", static_cast<int>(def.vk)));
    return h;
}

json::Value RuleToJson(const WindowRule& r) {
    auto obj = json::Value::MakeObject();
    obj.Set("id", WideToUtf8(r.id));
    obj.Set("name", WideToUtf8(r.name));
    obj.Set("enabled", r.enabled);
    obj.Set("action", ActionToStr(r.action));
    obj.Set("actionDelayMs", r.actionDelayMs);

    auto match = json::Value::MakeObject();
    match.Set("matchProcess", r.matchProcess);
    match.Set("processName", WideToUtf8(r.processName));
    match.Set("matchClass", r.matchClass);
    match.Set("className", WideToUtf8(r.className));
    match.Set("titleMode", TitleModeToStr(r.titleMode));
    match.Set("titlePattern", WideToUtf8(r.titlePattern));
    match.Set("excludePopupsAndDialogs", r.excludePopupsAndDialogs);
    match.Set("minSourceWidth", r.minSourceWidth);
    match.Set("minSourceHeight", r.minSourceHeight);
    obj.Set("match", std::move(match));

    auto geom = json::Value::MakeObject();
    geom.Set("mode", GeometryModeToStr(r.geometryMode));
    geom.Set("monitorMode", MonitorModeToStr(r.monitorMode));
    geom.Set("monitorIndex", r.monitorIndex);
    geom.Set("x", r.x);
    geom.Set("y", r.y);
    geom.Set("width", r.width);
    geom.Set("height", r.height);
    geom.Set("xPct", r.xPct);
    geom.Set("yPct", r.yPct);
    geom.Set("widthPct", r.widthPct);
    geom.Set("heightPct", r.heightPct);
    obj.Set("geometry", std::move(geom));

    auto opts = json::Value::MakeObject();
    opts.Set("makeResizable", r.makeResizable);
    opts.Set("borderless", r.borderless);
    opts.Set("alwaysOnTop", r.alwaysOnTop);
    opts.Set("compensateDwmMargin", r.compensateDwmMargin);
    opts.Set("aggressiveForce", r.aggressiveForce);
    opts.Set("useHookDll", r.useHookDll);
    opts.Set("unlockHitTest", r.unlockHitTest);
    opts.Set("lockTargetSize", r.lockTargetSize);
    opts.Set("customMinMax", r.customMinMax);
    opts.Set("customMinW", r.customMinW);
    opts.Set("customMinH", r.customMinH);
    opts.Set("customMaxW", r.customMaxW);
    opts.Set("customMaxH", r.customMaxH);
    opts.Set("scaleChildControls", r.scaleChildControls);
    opts.Set("enableOpacity", r.enableOpacity);
    opts.Set("opacityPercent", r.opacityPercent);
    opts.Set("smoothOpacityFade", r.smoothOpacityFade);
    obj.Set("options", std::move(opts));

    obj.Set("hotkey", HotkeyToJson(r.hotkey));
    return obj;
}

WindowRule JsonToRule(const json::Value& obj, bool regenerateId = false) {
    WindowRule r;
    r.id = regenerateId ? ConfigManager::GenerateRuleId() : Utf8ToWide(obj.GetString("id"));
    if (r.id.empty()) r.id = ConfigManager::GenerateRuleId();
    r.name = Utf8ToWide(obj.GetString("name", "Kural"));
    r.enabled = obj.GetBool("enabled", true);
    r.action = StrToAction(obj.GetString("action", "normal"));
    r.actionDelayMs = std::clamp(obj.GetInt("actionDelayMs", 0), 0, 60000);

    if (const auto* match = obj.Find("match"); match && match->IsObject()) {
        r.matchProcess = match->GetBool("matchProcess", true);
        r.processName = Utf8ToWide(match->GetString("processName"));
        r.matchClass = match->GetBool("matchClass", false);
        r.className = Utf8ToWide(match->GetString("className"));
        r.titleMode = StrToTitleMode(match->GetString("titleMode", "ignore"));
        r.titlePattern = Utf8ToWide(match->GetString("titlePattern"));
        r.excludePopupsAndDialogs = match->GetBool("excludePopupsAndDialogs", true);
        r.minSourceWidth = match->GetInt("minSourceWidth", 120);
        r.minSourceHeight = match->GetInt("minSourceHeight", 80);
    }

    if (const auto* geom = obj.Find("geometry"); geom && geom->IsObject()) {
        r.geometryMode = StrToGeometryMode(geom->GetString("mode", "percent"));
        r.monitorMode = StrToMonitorMode(geom->GetString("monitorMode", "windowCurrent"));
        r.monitorIndex = geom->GetInt("monitorIndex", 0);
        r.x = geom->GetInt("x", 100);
        r.y = geom->GetInt("y", 100);
        r.width = geom->GetInt("width", 1280);
        r.height = geom->GetInt("height", 800);
        r.xPct = geom->GetDouble("xPct", 0.0);
        r.yPct = geom->GetDouble("yPct", 0.0);
        r.widthPct = geom->GetDouble("widthPct", 50.0);
        r.heightPct = geom->GetDouble("heightPct", 100.0);
    }

    if (const auto* opts = obj.Find("options"); opts && opts->IsObject()) {
        r.makeResizable = opts->GetBool("makeResizable", true);
        r.borderless = opts->GetBool("borderless", false);
        r.alwaysOnTop = opts->GetBool("alwaysOnTop", false);
        r.compensateDwmMargin = opts->GetBool("compensateDwmMargin", true);
        r.aggressiveForce = opts->GetBool("aggressiveForce", true);
        r.useHookDll = opts->GetBool("useHookDll", true);
        r.unlockHitTest = opts->GetBool("unlockHitTest", false);
        r.lockTargetSize = opts->GetBool("lockTargetSize", false);
        r.customMinMax = opts->GetBool("customMinMax", false);
        r.customMinW = opts->GetInt("customMinW", 320);
        r.customMinH = opts->GetInt("customMinH", 240);
        r.customMaxW = opts->GetInt("customMaxW", 0);
        r.customMaxH = opts->GetInt("customMaxH", 0);
        r.scaleChildControls = opts->GetBool("scaleChildControls", false);
        r.enableOpacity = opts->GetBool("enableOpacity", false);
        r.opacityPercent = std::clamp(opts->GetInt("opacityPercent", 85), 10, 100);
        r.smoothOpacityFade = opts->GetBool("smoothOpacityFade", true);
    }

    r.hotkey = JsonToHotkey(obj.Find("hotkey"), HotkeyBinding{});
    return r;
}

json::Value ProfileToJson(const Profile& prof) {
    auto pObj = json::Value::MakeObject();
    pObj.Set("name", WideToUtf8(prof.name));
    auto rulesArr = json::Value::MakeArray();
    for (const auto& rule : prof.rules) {
        rulesArr.PushBack(RuleToJson(rule));
    }
    pObj.Set("rules", std::move(rulesArr));
    return pObj;
}

Profile JsonToProfile(const json::Value& pVal, bool regenerateRuleIds = false) {
    Profile prof;
    prof.name = Utf8ToWide(pVal.GetString("name", "Profil"));
    if (const auto* rules = pVal.Find("rules"); rules && rules->IsArray()) {
        for (const auto& rVal : rules->AsArray()) {
            if (rVal.IsObject()) {
                prof.rules.push_back(JsonToRule(rVal, regenerateRuleIds));
            }
        }
    }
    return prof;
}

bool ReadUtf8File(const std::wstring& path, std::string& outRaw, std::wstring* outError) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (outError) *outError = L"Dosya açılamadı / Cannot open file.";
        return false;
    }

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart > 16 * 1024 * 1024) {
        CloseHandle(hFile);
        if (outError) *outError = L"Dosya boyutu okunamadı veya çok büyük / File too large.";
        return false;
    }

    outRaw.assign(static_cast<size_t>(fileSize.QuadPart), '\0');
    DWORD readBytes = 0;
    if (fileSize.QuadPart > 0) {
        if (!ReadFile(hFile, &outRaw[0], static_cast<DWORD>(fileSize.QuadPart), &readBytes, nullptr)) {
            CloseHandle(hFile);
            if (outError) *outError = L"Dosya okuma hatası / Read error.";
            return false;
        }
        outRaw.resize(readBytes);
    }
    CloseHandle(hFile);
    return true;
}

bool WriteUtf8FileAtomic(const std::wstring& path, const std::string& data, std::wstring* outError) {
    const std::wstring tmpPath = path + L".tmp";
    HANDLE hFile = CreateFileW(tmpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (outError) *outError = L"Dosya yazılamadı / Cannot write file.";
        return false;
    }

    DWORD written = 0;
    const BOOL ok = WriteFile(hFile, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
    CloseHandle(hFile);

    if (!ok || written != data.size()) {
        DeleteFileW(tmpPath.c_str());
        if (outError) *outError = L"Yazma işlemi tamamlanamadı / Write failed.";
        return false;
    }

    if (!MoveFileExW(tmpPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmpPath.c_str());
        if (outError) *outError = L"Dosya güncellenemedi / Replace failed.";
        return false;
    }
    return true;
}

json::Value ConfigToJson(const AppConfig& config) {
    auto root = json::Value::MakeObject();
    root.Set("schemaVersion", config.schemaVersion);
    root.Set("language", WideToUtf8(config.language.empty() ? L"tr" : config.language));
    root.Set("monitoringEnabled", config.monitoringEnabled);
    root.Set("startMinimized", config.startMinimized);
    root.Set("minimizeToTrayOnClose", config.minimizeToTrayOnClose);
    root.Set("enableHookEngine", config.enableHookEngine);
    root.Set("activeProfileIndex", config.activeProfileIndex);

    auto hk = json::Value::MakeObject();
    hk.Set("unlockActiveWindow", HotkeyToJson(config.hotkeys.unlockActiveWindow));
    hk.Set("toggleBorderlessActive", HotkeyToJson(config.hotkeys.toggleBorderlessActive));
    hk.Set("toggleTopMostActive", HotkeyToJson(config.hotkeys.toggleTopMostActive));
    hk.Set("applyAllRules", HotkeyToJson(config.hotkeys.applyAllRules));
    root.Set("hotkeys", std::move(hk));

    auto profs = json::Value::MakeArray();
    for (const auto& prof : config.profiles) {
        profs.PushBack(ProfileToJson(prof));
    }
    root.Set("profiles", std::move(profs));
    return root;
}

constexpr wchar_t kStartupRegPath[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kStartupValueName[] = L"FlexWin";

} // namespace

std::wstring ConfigManager::GetExeDirectory() {
    wchar_t path[MAX_PATH]{};
    DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return L".";
    std::wstring s(path, len);
    size_t pos = s.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : s.substr(0, pos);
}

std::wstring ConfigManager::GetDefaultConfigPath() {
    return GetExeDirectory() + L"\\rules.json";
}

std::wstring ConfigManager::GenerateRuleId() {
    static std::mt19937_64 rng{GetTickCount64() ^ static_cast<uint64_t>(GetCurrentProcessId())};
    uint64_t v = rng();
    wchar_t buf[32]{};
    swprintf_s(buf, L"r_%08llx", static_cast<unsigned long long>(v & 0xFFFFFFFFull));
    return buf;
}

AppConfig ConfigManager::CreateDefaultConfig() {
    AppConfig cfg;
    cfg.schemaVersion = 1;
    cfg.language = L"tr";
    cfg.monitoringEnabled = true;
    cfg.startMinimized = false;
    cfg.minimizeToTrayOnClose = true;
    cfg.runAtStartup = IsRunAtStartupEnabled();
    cfg.enableHookEngine = true;
    cfg.activeProfileIndex = 0;

    Profile p1;
    p1.name = L"Kurumsal Varsayılan";

    WindowRule sample;
    sample.id = GenerateRuleId();
    sample.name = L"Örnek: Sabit Kurumsal Pencere (Devre Dışı)";
    sample.enabled = false;
    sample.matchProcess = true;
    sample.processName = L"ornek_erp.exe";
    sample.geometryMode = GeometryMode::Percent;
    sample.xPct = 5.0;
    sample.yPct = 5.0;
    sample.widthPct = 90.0;
    sample.heightPct = 90.0;
    sample.makeResizable = true;
    sample.useHookDll = true;
    sample.aggressiveForce = true;
    p1.rules.push_back(std::move(sample));

    Profile p2;
    p2.name = L"Çift Ekran Çalışma";

    cfg.profiles.push_back(std::move(p1));
    cfg.profiles.push_back(std::move(p2));
    return cfg;
}

bool ConfigManager::Load(const std::wstring& path, AppConfig& outConfig, std::wstring* outError) {
    std::string raw;
    if (!ReadUtf8File(path, raw, outError)) {
        return false;
    }

    json::Value root;
    std::string parseErr;
    if (!json::Parse(raw, root, &parseErr) || !root.IsObject()) {
        if (outError) *outError = L"JSON çözümleme hatası: " + Utf8ToWide(parseErr);
        return false;
    }

    AppConfig cfg;
    cfg.schemaVersion = root.GetInt("schemaVersion", 1);
    cfg.language = Utf8ToWide(root.GetString("language", "tr"));
    if (cfg.language != L"en" && cfg.language != L"tr") cfg.language = L"tr";
    cfg.monitoringEnabled = root.GetBool("monitoringEnabled", true);
    cfg.startMinimized = root.GetBool("startMinimized", false);
    cfg.minimizeToTrayOnClose = root.GetBool("minimizeToTrayOnClose", true);
    cfg.runAtStartup = IsRunAtStartupEnabled();
    cfg.enableHookEngine = root.GetBool("enableHookEngine", true);
    cfg.activeProfileIndex = root.GetInt("activeProfileIndex", 0);

    if (const auto* hk = root.Find("hotkeys"); hk && hk->IsObject()) {
        GlobalHotkeys defHk;
        cfg.hotkeys.unlockActiveWindow = JsonToHotkey(hk->Find("unlockActiveWindow"), defHk.unlockActiveWindow);
        cfg.hotkeys.toggleBorderlessActive = JsonToHotkey(hk->Find("toggleBorderlessActive"), defHk.toggleBorderlessActive);
        cfg.hotkeys.toggleTopMostActive = JsonToHotkey(hk->Find("toggleTopMostActive"), defHk.toggleTopMostActive);
        cfg.hotkeys.applyAllRules = JsonToHotkey(hk->Find("applyAllRules"), defHk.applyAllRules);
    }

    if (const auto* profs = root.Find("profiles"); profs && profs->IsArray()) {
        for (const auto& pVal : profs->AsArray()) {
            if (pVal.IsObject()) {
                cfg.profiles.push_back(JsonToProfile(pVal, false));
            }
        }
    }

    if (cfg.profiles.empty()) {
        Profile def;
        def.name = L"Kurumsal Varsayılan";
        cfg.profiles.push_back(std::move(def));
    }
    cfg.activeProfileIndex = std::clamp(cfg.activeProfileIndex, 0,
                                        static_cast<int>(cfg.profiles.size()) - 1);

    outConfig = std::move(cfg);
    return true;
}

bool ConfigManager::Save(const std::wstring& path, const AppConfig& config, std::wstring* outError) {
    const std::string dumped = ConfigToJson(config).Dump(2) + "\n";
    return WriteUtf8FileAtomic(path, dumped, outError);
}

bool ConfigManager::ExportProfile(const std::wstring& path, const Profile& profile, std::wstring* outError) {
    auto root = json::Value::MakeObject();
    root.Set("flexwinExportType", "profile");
    root.Set("schemaVersion", 1);
    root.Set("profile", ProfileToJson(profile));
    return WriteUtf8FileAtomic(path, root.Dump(2) + "\n", outError);
}

bool ConfigManager::ExportAllProfiles(const std::wstring& path, const AppConfig& config, std::wstring* outError) {
    auto root = ConfigToJson(config);
    root.Set("flexwinExportType", "fullBackup");
    return WriteUtf8FileAtomic(path, root.Dump(2) + "\n", outError);
}

bool ConfigManager::ImportProfiles(const std::wstring& path, std::vector<Profile>& outProfiles, std::wstring* outError) {
    std::string raw;
    if (!ReadUtf8File(path, raw, outError)) {
        return false;
    }

    json::Value root;
    std::string parseErr;
    if (!json::Parse(raw, root, &parseErr) || !root.IsObject()) {
        if (outError) *outError = L"JSON çözümleme hatası / JSON parse error: " + Utf8ToWide(parseErr);
        return false;
    }

    std::vector<Profile> imported;
    if (const auto* single = root.Find("profile"); single && single->IsObject()) {
        imported.push_back(JsonToProfile(*single, true));
    } else if (const auto* profs = root.Find("profiles"); profs && profs->IsArray()) {
        for (const auto& pVal : profs->AsArray()) {
            if (pVal.IsObject()) {
                imported.push_back(JsonToProfile(pVal, true));
            }
        }
    } else if (root.Find("rules") && root.Find("rules")->IsArray()) {
        imported.push_back(JsonToProfile(root, true));
    }

    if (imported.empty()) {
        if (outError) *outError = L"Dosyada geçerli profil bulunamadı / No valid profiles found in file.";
        return false;
    }

    outProfiles = std::move(imported);
    return true;
}

bool ConfigManager::SetRunAtStartup(bool enable) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kStartupRegPath, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    bool ok = false;
    if (enable) {
        wchar_t exePath[MAX_PATH]{};
        DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH) {
            std::wstring val = L"\"" + std::wstring(exePath, len) + L"\" --minimized";
            ok = (RegSetValueExW(hKey, kStartupValueName, 0, REG_SZ,
                                 reinterpret_cast<const BYTE*>(val.c_str()),
                                 static_cast<DWORD>((val.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS);
        }
    } else {
        LONG res = RegDeleteValueW(hKey, kStartupValueName);
        ok = (res == ERROR_SUCCESS || res == ERROR_FILE_NOT_FOUND);
    }
    RegCloseKey(hKey);
    return ok;
}

bool ConfigManager::IsRunAtStartupEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kStartupRegPath, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0;
    DWORD size = 0;
    LONG res = RegQueryValueExW(hKey, kStartupValueName, nullptr, &type, nullptr, &size);
    RegCloseKey(hKey);
    return (res == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ));
}

} // namespace flexwin
