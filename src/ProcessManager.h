#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <memory>

struct ProcessInfo {
    DWORD pid = 0;
    std::wstring processName;
    std::wstring windowTitle;
    HWND hwnd = nullptr;
    bool is64Bit = false;
};

class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();

    std::vector<ProcessInfo> EnumerateGameProcesses();
    bool OpenTarget(DWORD pid);
    void CloseTarget();

    bool IsAttached() const;
    HANDLE GetHandle() const { return m_hProcess; }
    DWORD GetPID() const { return m_currentPid; }
    const ProcessInfo& GetCurrentProcessInfo() const { return m_currentInfo; }
    bool GetTargetWindowRect(RECT& outRect) const;
    HWND GetTargetHwnd() const { return m_currentInfo.hwnd; }

private:
    HANDLE m_hProcess = nullptr;
    DWORD m_currentPid = 0;
    ProcessInfo m_currentInfo;

    static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam);
    static bool IsProcess64Bit(HANDLE hProcess);
};
