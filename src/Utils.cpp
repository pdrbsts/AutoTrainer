#include "Utils.h"
#include <algorithm>
#include <cctype>
#include <shellapi.h>

void Logger::Log(const std::string& msg, LogLevel level) {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm timeInfo;
    localtime_s(&timeInfo, &in_time_t);

    std::ostringstream ss;
    ss << std::put_time(&timeInfo, "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.push_back({ ss.str(), msg, level });
    if (m_entries.size() > 1000) {
        m_entries.erase(m_entries.begin(), m_entries.begin() + 100);
    }

    OutputDebugStringA((msg + "\n").c_str());
}

void Logger::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

std::vector<LogEntry> Logger::GetEntries() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entries;
}

namespace StringUtils {
    std::string WideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(sizeNeeded, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], sizeNeeded, NULL, NULL);
        return strTo;
    }

    std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return std::wstring();
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], sizeNeeded);
        return wstrTo;
    }

    std::string FormatAddress(uintptr_t address) {
        std::ostringstream ss;
#if defined(_WIN64)
        ss << "0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(16) << address;
#else
        ss << "0x" << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << address;
#endif
        return ss.str();
    }

    std::string FormatNumberWithCommas(int64_t number) {
        std::string str = std::to_string(std::abs(number));
        int insertPosition = (int)str.length() - 3;
        while (insertPosition > 0) {
            str.insert(insertPosition, ",");
            insertPosition -= 3;
        }
        if (number < 0) str.insert(0, "-");
        return str;
    }

    std::string Trim(const std::string& str) {
        auto start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        auto end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    bool ParseInt64(const std::string& str, int64_t& outValue) {
        std::string cleaned;
        for (char c : str) {
            if (isdigit((unsigned char)c) || c == '-') {
                cleaned += c;
            }
        }
        if (cleaned.empty() || cleaned == "-") return false;
        try {
            outValue = std::stoll(cleaned);
            return true;
        } catch (...) {
            return false;
        }
    }

    bool ParseDouble(const std::string& str, double& outValue) {
        std::string cleaned;
        bool hasDot = false;
        for (char c : str) {
            if (isdigit((unsigned char)c) || c == '-') {
                cleaned += c;
            } else if ((c == '.' || c == ',') && !hasDot) {
                cleaned += '.';
                hasDot = true;
            }
        }
        if (cleaned.empty() || cleaned == "-" || cleaned == ".") return false;
        try {
            outValue = std::stod(cleaned);
            return true;
        } catch (...) {
            return false;
        }
    }
}

namespace PrivilegeUtils {
    bool IsRunningAsAdmin() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = nullptr;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                     DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(nullptr, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        return isAdmin != FALSE;
    }

    bool RelaunchAsAdmin(HWND hwndParent, const wchar_t* cmdLine) {
        wchar_t szExePath[MAX_PATH];
        if (!GetModuleFileNameW(nullptr, szExePath, MAX_PATH)) {
            return false;
        }

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.cbSize = sizeof(sei);
        sei.hwnd = hwndParent;
        sei.lpVerb = L"runas";
        sei.lpFile = szExePath;
        sei.lpParameters = (cmdLine && cmdLine[0] != L'\0') ? cmdLine : nullptr;
        sei.nShow = SW_NORMAL;

        if (!ShellExecuteExW(&sei)) {
            return false;
        }
        return true;
    }

    bool EnableDebugPrivilege() {
        HANDLE hToken = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            return false;
        }

        LUID luid;
        if (!LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &luid)) {
            CloseHandle(hToken);
            return false;
        }

        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        BOOL res = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr);
        DWORD err = GetLastError();
        CloseHandle(hToken);

        return (res && err != ERROR_NOT_ALL_ASSIGNED);
    }
}

