#include "Logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <cstdio>

namespace flexwin {

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

void Logger::Log(LogLevel level, const std::wstring& msg) {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t ts[32]{};
    swprintf_s(ts, L"%02u:%02u:%02u.%03u", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    LogEntry entry{ts, level, msg};
    std::function<void(const LogEntry&)> cbCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_entries.size() >= kMaxEntries) {
            m_entries.pop_front();
        }
        m_entries.push_back(entry);
        cbCopy = m_callback;
    }
    if (cbCopy) {
        cbCopy(entry);
    }
}

std::vector<LogEntry> Logger::Snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return {m_entries.begin(), m_entries.end()};
}

LogEntry Logger::Latest() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_entries.empty()) return {};
    return m_entries.back();
}

void Logger::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

void Logger::SetCallback(std::function<void(const LogEntry&)> cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(cb);
}

} // namespace flexwin
