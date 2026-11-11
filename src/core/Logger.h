#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace flexwin {

enum class LogLevel {
    Info,
    Success,
    Warning,
    Error
};

struct LogEntry {
    std::wstring timestamp;
    LogLevel level = LogLevel::Info;
    std::wstring message;
};

class Logger {
public:
    static Logger& Instance();

    void Log(LogLevel level, const std::wstring& msg);
    void Info(const std::wstring& msg) { Log(LogLevel::Info, msg); }
    void Success(const std::wstring& msg) { Log(LogLevel::Success, msg); }
    void Warn(const std::wstring& msg) { Log(LogLevel::Warning, msg); }
    void Error(const std::wstring& msg) { Log(LogLevel::Error, msg); }

    std::vector<LogEntry> Snapshot() const;
    LogEntry Latest() const;
    void Clear();

    void SetCallback(std::function<void(const LogEntry&)> cb);

private:
    Logger() = default;
    mutable std::mutex m_mutex;
    std::deque<LogEntry> m_entries;
    std::function<void(const LogEntry&)> m_callback;
    static constexpr size_t kMaxEntries = 500;
};

} // namespace flexwin
