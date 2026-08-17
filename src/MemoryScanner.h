#pragma once
#include <windows.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <cstdint>
#include <string>

enum class ScanDataType {
    Int32,
    Int64,
    Float,
    Double
};

struct ScanValue {
    ScanDataType type = ScanDataType::Int32;
    int64_t intVal = 0;
    double doubleVal = 0.0;

    bool Matches(const void* buffer, size_t offset) const;
    std::string ToString() const;
};

struct CandidateAddress {
    uintptr_t address = 0;
    int64_t currentValueInt = 0;
    int64_t previousValueInt = 0;
    double currentValueDouble = 0.0;
    double previousValueDouble = 0.0;
    bool isLocked = false;
    int64_t lockValueInt = 0;
    double lockValueDouble = 0.0;
};

class MemoryScanner {
public:
    MemoryScanner();
    ~MemoryScanner();

    void SetProcessHandle(HANDLE hProcess, bool is64Bit);

    // Scans
    void StartFirstScan(const ScanValue& targetValue, std::function<void(size_t)> onComplete = nullptr);
    void StartNextScan(const ScanValue& targetValue, std::function<void(size_t)> onComplete = nullptr);
    void CancelScan();
    void Reset();

    // Candidates
    std::vector<CandidateAddress> GetCandidates() const;
    size_t GetCandidateCount() const;
    void RefreshCandidateValues();

    // Memory manipulation
    bool WriteValue(uintptr_t address, const ScanValue& value);
    bool ReadValue(uintptr_t address, ScanDataType type, ScanValue& outVal);

    // Freeze / Lock
    void SetAddressLock(uintptr_t address, bool lock, const ScanValue& val);
    void LockThreadLoop();

    // Status
    bool IsScanning() const { return m_isScanning; }
    float GetProgress() const { return m_progress; }
    ScanDataType GetCurrentDataType() const { return m_currentDataType; }
    void SetDataType(ScanDataType type) { m_currentDataType = type; }
    bool GetAlignToType() const { return m_alignToType; }
    void SetAlignToType(bool align) { m_alignToType = align; }

private:
    HANDLE m_hProcess = nullptr;
    bool m_is64Bit = true;
    ScanDataType m_currentDataType = ScanDataType::Int32;
    bool m_alignToType = true;

    mutable std::mutex m_candidatesMutex;
    std::vector<CandidateAddress> m_candidates;

    std::atomic<bool> m_isScanning{ false };
    std::atomic<bool> m_cancelRequested{ false };
    std::atomic<float> m_progress{ 0.0f };

    std::thread m_scanThread;
    std::thread m_lockThread;
    std::atomic<bool> m_lockThreadRunning{ true };

    void DoFirstScan(ScanValue targetValue, std::function<void(size_t)> onComplete);
    void DoNextScan(ScanValue targetValue, std::function<void(size_t)> onComplete);
};
