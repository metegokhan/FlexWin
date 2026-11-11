#pragma once

#include <string>
#include <vector>
#include "Models.h"

namespace flexwin {

class ConfigManager {
public:
    static std::wstring GetExeDirectory();
    static std::wstring GetDefaultConfigPath();
    static std::wstring GenerateRuleId();

    static AppConfig CreateDefaultConfig();
    static bool Load(const std::wstring& path, AppConfig& outConfig, std::wstring* outError = nullptr);
    static bool Save(const std::wstring& path, const AppConfig& config, std::wstring* outError = nullptr);

    static bool ExportProfile(const std::wstring& path, const Profile& profile, std::wstring* outError = nullptr);
    static bool ExportAllProfiles(const std::wstring& path, const AppConfig& config, std::wstring* outError = nullptr);
    static bool ImportProfiles(const std::wstring& path, std::vector<Profile>& outProfiles, std::wstring* outError = nullptr);

    static bool SetRunAtStartup(bool enable);
    static bool IsRunAtStartupEnabled();
};

} // namespace flexwin
