#include <cassert>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "../src/core/ConfigManager.h"
#include "../src/core/I18n.h"
#include "../src/core/MonitorManager.h"
#include "../src/core/StringUtil.h"
#include "../src/core/WindowEngine.h"
#include "../src/json/Json.h"

using namespace flexwin;

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool cond, const char* name) {
    if (cond) {
        ++g_passed;
    } else {
        ++g_failed;
        std::cerr << "[FAIL] " << name << "\n";
    }
}

void TestJsonRoundTrip() {
    json::Value obj = json::Value::MakeObject();
    obj.Set("name", WideToUtf8(L"Kurumsal ERP — Türkçe ğüşıöçĞÜŞİÖÇ"));
    obj.Set("count", 42);
    obj.Set("ratio", 12.5);
    obj.Set("enabled", true);
    auto arr = json::Value::MakeArray();
    arr.PushBack("a\\b\"c\n");
    arr.PushBack(nullptr);
    obj.Set("items", std::move(arr));

    const std::string dumped = obj.Dump(2);
    json::Value parsed;
    std::string err;
    Check(json::Parse(dumped, parsed, &err), "JSON parse round-trip");
    Check(Utf8ToWide(parsed.GetString("name")) == L"Kurumsal ERP — Türkçe ğüşıöçĞÜŞİÖÇ", "JSON UTF-8 Turkish");
    Check(parsed.GetInt("count") == 42, "JSON integer");
    Check(parsed.GetDouble("ratio") == 12.5, "JSON double");
    Check(parsed.GetBool("enabled") == true, "JSON bool");
    const auto* items = parsed.Find("items");
    Check(items && items->IsArray() && items->AsArray().size() == 2, "JSON array");
    Check(items && items->AsArray()[0].AsString() == "a\\b\"c\n", "JSON string escapes");

    json::Value bad;
    Check(!json::Parse("{bad json", bad, &err), "JSON rejects malformed input");
}

void TestConfigRoundTrip() {
    AppConfig cfg = ConfigManager::CreateDefaultConfig();
    cfg.language = L"en";
    cfg.monitoringEnabled = false;
    cfg.enableHookEngine = true;
    cfg.profiles[0].rules[0].enabled = true;
    cfg.profiles[0].rules[0].processName = L"saplogon.exe;erp_client.exe";
    cfg.profiles[0].rules[0].titleMode = TitleMatchMode::Regex;
    cfg.profiles[0].rules[0].titlePattern = L"Fatura.*2026";
    cfg.profiles[0].rules[0].action = WindowAction::AutoMinimize;
    cfg.profiles[0].rules[0].actionDelayMs = 450;
    cfg.profiles[0].rules[0].enableOpacity = true;
    cfg.profiles[0].rules[0].opacityPercent = 70;
    cfg.profiles[0].rules[0].smoothOpacityFade = false;
    cfg.profiles[0].rules[0].borderless = true;
    cfg.profiles[0].rules[0].unlockHitTest = true;
    cfg.profiles[0].rules[0].scaleChildControls = true;

    wchar_t tmpDir[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmpDir);
    const std::wstring testPath = std::wstring(tmpDir) + L"flexwin_unit_test_rules.json";

    std::wstring err;
    Check(ConfigManager::Save(testPath, cfg, &err), "Config save");

    AppConfig loaded;
    Check(ConfigManager::Load(testPath, loaded, &err), "Config load");
    DeleteFileW(testPath.c_str());

    Check(loaded.language == L"en", "Config language");
    Check(loaded.monitoringEnabled == false, "Config monitoringEnabled");
    Check(loaded.profiles.size() == 2, "Config profiles count");
    Check(loaded.profiles[0].rules[0].processName == L"saplogon.exe;erp_client.exe", "Config processName");
    Check(loaded.profiles[0].rules[0].titleMode == TitleMatchMode::Regex, "Config titleMode");
    Check(loaded.profiles[0].rules[0].titlePattern == L"Fatura.*2026", "Config titlePattern");
    Check(loaded.profiles[0].rules[0].action == WindowAction::AutoMinimize, "Config action");
    Check(loaded.profiles[0].rules[0].actionDelayMs == 450, "Config actionDelayMs");
    Check(loaded.profiles[0].rules[0].enableOpacity == true, "Config enableOpacity");
    Check(loaded.profiles[0].rules[0].opacityPercent == 70, "Config opacityPercent");
    Check(loaded.profiles[0].rules[0].smoothOpacityFade == false, "Config smoothOpacityFade");
    Check(loaded.profiles[0].rules[0].borderless == true, "Config borderless");
    Check(loaded.profiles[0].rules[0].unlockHitTest == true, "Config unlockHitTest");
    Check(loaded.profiles[0].rules[0].scaleChildControls == true, "Config scaleChildControls");

    // Profile Export & Import tests
    const std::wstring exportSinglePath = std::wstring(tmpDir) + L"flexwin_export_single.json";
    const std::wstring exportAllPath = std::wstring(tmpDir) + L"flexwin_export_all.json";
    Check(ConfigManager::ExportProfile(exportSinglePath, cfg.profiles[0], &err), "Export single profile");
    std::vector<Profile> importedSingle;
    Check(ConfigManager::ImportProfiles(exportSinglePath, importedSingle, &err), "Import single profile");
    DeleteFileW(exportSinglePath.c_str());
    Check(importedSingle.size() == 1 && importedSingle[0].name == cfg.profiles[0].name, "Imported single profile name");
    Check(importedSingle.size() == 1 && importedSingle[0].rules.size() == 1 &&
              importedSingle[0].rules[0].opacityPercent == 70 &&
              importedSingle[0].rules[0].id != cfg.profiles[0].rules[0].id,
          "Imported rule preserves fields and generates fresh ID");

    Check(ConfigManager::ExportAllProfiles(exportAllPath, cfg, &err), "Export all profiles");
    std::vector<Profile> importedAll;
    Check(ConfigManager::ImportProfiles(exportAllPath, importedAll, &err), "Import all profiles");
    DeleteFileW(exportAllPath.c_str());
    Check(importedAll.size() == 2, "Imported all profiles count");
}

void TestI18nSwitching() {
    I18n::SetLanguageCode(L"tr");
    Check(I18n::GetLanguage() == Language::Tr, "I18n TR active");
    const std::wstring trSave = Tr(StrKey::BtnSave);

    I18n::SetLanguageCode(L"en");
    Check(I18n::GetLanguage() == Language::En, "I18n EN active");
    const std::wstring enSave = Tr(StrKey::BtnSave);
    Check(!trSave.empty() && !enSave.empty() && trSave != enSave, "I18n TR/EN translations differ");
    I18n::SetLanguage(Language::Tr);
}

void TestRuleMatching() {
    WindowInfo win;
    win.processName = L"MyCorporateApp.exe";
    win.processPath = L"C:\\Program Files\\Corp\\MyCorporateApp.exe";
    win.className = L"SunAwtFrame";
    win.title = L"Finans Modülü - Rapor #204";
    win.windowRect = RECT{100, 100, 900, 700};

    WindowRule rule;
    rule.enabled = true;
    rule.matchProcess = true;
    rule.processName = L"other.exe;mycorporateapp.exe";
    rule.matchClass = false;
    rule.titleMode = TitleMatchMode::Contains;
    rule.titlePattern = L"Finans";
    Check(WindowEngine::MatchesRule(rule, win), "Match process list + contains title");

    rule.titleMode = TitleMatchMode::Regex;
    rule.titlePattern = L"Rapor #\\d+";
    Check(WindowEngine::MatchesRule(rule, win), "Match regex title");

    rule.titlePattern = L"[invalid(regex";
    Check(!WindowEngine::MatchesRule(rule, win), "Invalid regex fails safely without throwing");

    rule.titleMode = TitleMatchMode::Ignore;
    win.isDialogClass = true;
    win.className = L"#32770";
    Check(!WindowEngine::MatchesRule(rule, win), "Popup/dialog guard blocks #32770 by default");

    rule.matchClass = true;
    rule.className = L"#32770";
    Check(WindowEngine::MatchesRule(rule, win), "Explicit #32770 class match allows dialog");

    win.windowRect = RECT{0, 0, 50, 40};
    Check(!WindowEngine::MatchesRule(rule, win), "Min source size guard rejects tiny splash window");
}

void TestMonitorGeometryMath() {
    std::vector<MonitorInfoEx> mons(2);
    mons[0].isPrimary = true;
    mons[0].monitorRect = RECT{0, 0, 1920, 1080};
    mons[0].workRect = RECT{0, 0, 1920, 1040};
    mons[1].isPrimary = false;
    mons[1].monitorRect = RECT{1920, 0, 3840, 1080};
    mons[1].workRect = RECT{1920, 0, 3840, 1040};

    WindowRule r;
    r.geometryMode = GeometryMode::Percent;
    r.monitorMode = MonitorTargetMode::Index;
    r.monitorIndex = 1;
    r.xPct = 50.0;
    r.yPct = 0.0;
    r.widthPct = 50.0;
    r.heightPct = 100.0;

    RECT pr = MonitorManager::ComputePreviewRect(r, mons, 0);
    Check(pr.left == 2880 && pr.top == 0 && pr.right == 3840 && pr.bottom == 1040,
          "Percent layout on secondary monitor");

    r.geometryMode = GeometryMode::Pixels;
    r.x = 100;
    r.y = 50;
    r.width = 800;
    r.height = 600;
    pr = MonitorManager::ComputePreviewRect(r, mons, 0);
    Check(pr.left == 2020 && pr.top == 50 && pr.right == 2820 && pr.bottom == 650,
          "Pixel layout relative to secondary monitor work area");
}

} // namespace

int main() {
    TestJsonRoundTrip();
    TestConfigRoundTrip();
    TestI18nSwitching();
    TestRuleMatching();
    TestMonitorGeometryMath();

    std::cout << "Passed: " << g_passed << ", Failed: " << g_failed << "\n";
    return (g_failed == 0) ? 0 : 1;
}
