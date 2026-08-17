#include "ProcessManager.h"
#include "Utils.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <algorithm>

ProcessManager::ProcessManager() {
}

ProcessManager::~ProcessManager() {
    CloseTarget();
}

BOOL CALLBACK ProcessManager::EnumWindowsCallback(HWND hwnd, LPARAM lParam) {
    auto list = reinterpret_cast<std::vector<ProcessInfo>*>(lParam);

    if (!IsWindowVisible(hwnd)) return TRUE;

    // Check if minimized/cloaked
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(hwnd, &wp);

    RECT r;
    GetWindowRect(hwnd, &r);
    if ((r.right - r.left <= 0) || (r.bottom - r.top <= 0)) return TRUE;

    int length = GetWindowTextLengthW(hwnd);
    if (length == 0) return TRUE;

    std::wstring title(length + 1, L'\0');
    GetWindowTextW(hwnd, &title[0], length + 1);
    title.resize(length);

    // Skip unwanted shell/system windows
    if (title == L"Program Manager" || title == L"Settings" || title == L"Windows Input Experience" || title == L"AutoTrainer") {
        return TRUE;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid == GetCurrentProcessId()) return TRUE;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) {
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    }

    std::wstring processName = L"Unknown";
    bool is64 = false;

    if (hProcess) {
        wchar_t imagePath[MAX_PATH] = { 0 };
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, imagePath, &size)) {
            std::wstring fullPath(imagePath);
            size_t lastSlash = fullPath.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos) {
                processName = fullPath.substr(lastSlash + 1);
            } else {
                processName = fullPath;
            }
        }
        is64 = IsProcess64Bit(hProcess);
        CloseHandle(hProcess);
    }

    ProcessInfo info;
    info.pid = pid;
    info.processName = processName;
    info.windowTitle = title;
    info.hwnd = hwnd;
    info.is64Bit = is64;

    // Avoid duplicate PID entries in list
    auto it = std::find_if(list->begin(), list->end(), [pid](const ProcessInfo& p) { return p.pid == pid; });
    if (it == list->end()) {
        list->push_back(info);
    }

    return TRUE;
}

bool ProcessManager::IsProcess64Bit(HANDLE hProcess) {
#if defined(_WIN64)
    BOOL isWow64 = FALSE;
    if (IsWow64Process(hProcess, &isWow64)) {
        return !isWow64; // If not Wow64 on 64-bit Windows, it's 64-bit
    }
    return true;
#else
    return false;
#endif
}

std::vector<ProcessInfo> ProcessManager::EnumerateGameProcesses() {
    std::vector<ProcessInfo> processes;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&processes));

    // Sort by process name alphabetically
    std::sort(processes.begin(), processes.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
        return a.processName < b.processName;
    });

    return processes;
}

bool ProcessManager::OpenTarget(DWORD pid) {
    CloseTarget();

    if (pid == 0) return false;

    // Try full access first, fallback to standard memory scan permissions
    m_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!m_hProcess) {
        m_hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, pid);
    }

    if (!m_hProcess) {
        LOG_ERROR("Failed to open target process PID " + std::to_string(pid) + " (Error: " + std::to_string(GetLastError()) + ")");
        return false;
    }

    m_currentPid = pid;

    // Find window info
    auto processes = EnumerateGameProcesses();
    for (const auto& proc : processes) {
        if (proc.pid == pid) {
            m_currentInfo = proc;
            break;
        }
    }

    if (m_currentInfo.pid != pid) {
        m_currentInfo.pid = pid;
        m_currentInfo.processName = L"PID " + std::to_wstring(pid);
        m_currentInfo.windowTitle = L"Attached Process";
        m_currentInfo.is64Bit = IsProcess64Bit(m_hProcess);
    }

    LOG_SUCCESS("Successfully attached to: " + StringUtils::WideToUtf8(m_currentInfo.processName) +
                " [PID: " + std::to_string(pid) + "] (" + (m_currentInfo.is64Bit ? "64-bit" : "32-bit") + ")");

    return true;
}

void ProcessManager::CloseTarget() {
    if (m_hProcess) {
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
    }
    m_currentPid = 0;
    m_currentInfo = ProcessInfo();
}

bool ProcessManager::IsAttached() const {
    if (!m_hProcess) return false;
    DWORD exitCode = 0;
    if (GetExitCodeProcess(m_hProcess, &exitCode)) {
        return (exitCode == STILL_ACTIVE);
    }
    return false;
}

bool ProcessManager::GetTargetWindowRect(RECT& outRect) const {
    if (m_currentInfo.hwnd && IsWindow(m_currentInfo.hwnd)) {
        return GetWindowRect(m_currentInfo.hwnd, &outRect) != 0;
    }
    return false;
}
