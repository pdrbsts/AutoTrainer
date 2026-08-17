#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <windows.h>

enum class LogLevel {
    Info,
    Success,
    Warning,
    Error
};

struct LogEntry {
    std::string timestamp;
    std::string message;
    LogLevel level;
};

class Logger {
public:
    static Logger& Get() {
        static Logger instance;
        return instance;
    }

    void Log(const std::string& msg, LogLevel level = LogLevel::Info);
    void Clear();
    std::vector<LogEntry> GetEntries();

private:
    std::mutex m_mutex;
    std::vector<LogEntry> m_entries;
};

#define LOG_INFO(msg) Logger::Get().Log(msg, LogLevel::Info)
#define LOG_SUCCESS(msg) Logger::Get().Log(msg, LogLevel::Success)
#define LOG_WARNING(msg) Logger::Get().Log(msg, LogLevel::Warning)
#define LOG_ERROR(msg) Logger::Get().Log(msg, LogLevel::Error)

namespace StringUtils {
    std::string WideToUtf8(const std::wstring& wstr);
    std::wstring Utf8ToWide(const std::string& str);
    std::string FormatAddress(uintptr_t address);
    std::string FormatNumberWithCommas(int64_t number);
    std::string Trim(const std::string& str);
    bool ParseInt64(const std::string& str, int64_t& outValue);
    bool ParseDouble(const std::string& str, double& outValue);
}
