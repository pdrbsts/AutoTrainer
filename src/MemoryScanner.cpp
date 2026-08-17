#include "MemoryScanner.h"
#include "Utils.h"
#include <cmath>
#include <algorithm>

bool ScanValue::Matches(const void* buffer, size_t offset) const {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(buffer) + offset;
    switch (type) {
    case ScanDataType::Int32: {
        int32_t val = *reinterpret_cast<const int32_t*>(ptr);
        return val == static_cast<int32_t>(intVal);
    }
    case ScanDataType::Int64: {
        int64_t val = *reinterpret_cast<const int64_t*>(ptr);
        return val == intVal;
    }
    case ScanDataType::Float: {
        float val = *reinterpret_cast<const float*>(ptr);
        return std::fabs(val - static_cast<float>(doubleVal)) < 0.001f;
    }
    case ScanDataType::Double: {
        double val = *reinterpret_cast<const double*>(ptr);
        return std::fabs(val - doubleVal) < 0.0001;
    }
    case ScanDataType::String: {
        if (stringVal.empty()) return false;
        return memcmp(ptr, stringVal.data(), stringVal.size()) == 0;
    }
    }
    return false;
}

std::string ScanValue::ToString() const {
    switch (type) {
    case ScanDataType::Int32: return std::to_string(static_cast<int32_t>(intVal));
    case ScanDataType::Int64: return std::to_string(intVal);
    case ScanDataType::Float: return std::to_string(static_cast<float>(doubleVal));
    case ScanDataType::Double: return std::to_string(doubleVal);
    case ScanDataType::String: return stringVal;
    }
    return "";
}

MemoryScanner::MemoryScanner() {
    m_lockThread = std::thread(&MemoryScanner::LockThreadLoop, this);
}

MemoryScanner::~MemoryScanner() {
    CancelScan();
    m_lockThreadRunning = false;
    if (m_lockThread.joinable()) {
        m_lockThread.join();
    }
}

void MemoryScanner::SetProcessHandle(HANDLE hProcess, bool is64Bit) {
    CancelScan();
    Reset();
    m_hProcess = hProcess;
    m_is64Bit = is64Bit;
}

void MemoryScanner::Reset() {
    std::lock_guard<std::mutex> lock(m_candidatesMutex);
    m_candidates.clear();
    m_progress = 0.0f;
}

void MemoryScanner::CancelScan() {
    m_cancelRequested = true;
    if (m_scanThread.joinable()) {
        m_scanThread.join();
    }
    m_isScanning = false;
    m_cancelRequested = false;
}

std::vector<CandidateAddress> MemoryScanner::GetCandidates() const {
    std::lock_guard<std::mutex> lock(m_candidatesMutex);
    return m_candidates;
}

size_t MemoryScanner::GetCandidateCount() const {
    std::lock_guard<std::mutex> lock(m_candidatesMutex);
    return m_candidates.size();
}

void MemoryScanner::StartFirstScan(const ScanValue& targetValue, std::function<void(size_t)> onComplete) {
    if (m_isScanning) return;
    if (m_scanThread.joinable()) m_scanThread.join();

    m_currentDataType = targetValue.type;
    m_isScanning = true;
    m_cancelRequested = false;
    m_progress = 0.0f;

    m_scanThread = std::thread(&MemoryScanner::DoFirstScan, this, targetValue, onComplete);
}

void MemoryScanner::StartNextScan(const ScanValue& targetValue, std::function<void(size_t)> onComplete) {
    if (m_isScanning) return;
    if (m_scanThread.joinable()) m_scanThread.join();

    m_currentDataType = targetValue.type;
    m_isScanning = true;
    m_cancelRequested = false;
    m_progress = 0.0f;

    m_scanThread = std::thread(&MemoryScanner::DoNextScan, this, targetValue, onComplete);
}

void MemoryScanner::DoFirstScan(ScanValue targetValue, std::function<void(size_t)> onComplete) {
    LOG_INFO("Starting First Scan for value: " + targetValue.ToString());
    auto startTime = std::chrono::high_resolution_clock::now();

    {
        std::lock_guard<std::mutex> lock(m_candidatesMutex);
        m_candidates.clear();
    }

    if (!m_hProcess) {
        m_isScanning = false;
        if (onComplete) onComplete(0);
        return;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    uintptr_t minAddress = 0x10000;
    uintptr_t maxAddress = m_is64Bit ? 0x7FFFFFFEFFFF : 0x7FFE0000;

    // Collect readable writable regions
    struct MemRegion {
        uintptr_t base;
        size_t size;
    };
    std::vector<MemRegion> regions;
    uintptr_t currentAddr = minAddress;

    while (currentAddr < maxAddress) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQueryEx(m_hProcess, reinterpret_cast<LPCVOID>(currentAddr), &mbi, sizeof(mbi)) == 0) {
            break;
        }

        uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (mbi.State == MEM_COMMIT && 
            !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY))) {
            regions.push_back({ reinterpret_cast<uintptr_t>(mbi.BaseAddress), mbi.RegionSize });
        }

        if (regionEnd <= currentAddr) break;
        currentAddr = regionEnd;
    }

    size_t totalBytes = 0;
    for (const auto& reg : regions) totalBytes += reg.size;
    size_t scannedBytes = 0;

    std::vector<CandidateAddress> foundList;
    foundList.reserve(10000);

    const size_t CHUNK_SIZE = 512 * 1024; // 512 KB
    std::vector<uint8_t> buffer(CHUNK_SIZE);

    size_t valSize = 4;
    size_t step = m_alignToType ? 4 : 1;
    if (targetValue.type == ScanDataType::Int64 || targetValue.type == ScanDataType::Double) {
        valSize = 8;
        if (m_alignToType) step = 8;
    } else if (targetValue.type == ScanDataType::String) {
        valSize = std::max((size_t)1, targetValue.stringVal.size());
        step = 1;
    }

    for (const auto& reg : regions) {
        if (m_cancelRequested) break;

        uintptr_t regOffset = 0;
        while (regOffset < reg.size && !m_cancelRequested) {
            size_t bytesToRead = std::min(CHUNK_SIZE, reg.size - regOffset);
            SIZE_T bytesRead = 0;

            if (ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(reg.base + regOffset), buffer.data(), bytesToRead, &bytesRead) && bytesRead >= valSize) {
                size_t limit = bytesRead - valSize;
                for (size_t i = 0; i <= limit; i += step) {
                    if (targetValue.Matches(buffer.data(), i)) {
                        CandidateAddress cand;
                        cand.address = reg.base + regOffset + i;
                        cand.currentValueInt = targetValue.intVal;
                        cand.previousValueInt = targetValue.intVal;
                        cand.currentValueDouble = targetValue.doubleVal;
                        cand.previousValueDouble = targetValue.doubleVal;
                        cand.currentValueString = targetValue.stringVal;
                        cand.previousValueString = targetValue.stringVal;
                        foundList.push_back(cand);
                    }
                }
            }

            regOffset += bytesToRead;
            scannedBytes += bytesToRead;
            if (totalBytes > 0) {
                m_progress = static_cast<float>(scannedBytes) / static_cast<float>(totalBytes);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_candidatesMutex);
        m_candidates = std::move(foundList);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double scanDurationMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    size_t finalCount = GetCandidateCount();
    m_progress = 1.0f;
    m_isScanning = false;

    LOG_SUCCESS("First scan complete: found " + StringUtils::FormatNumberWithCommas(finalCount) + 
                " candidates in " + std::to_string((int)scanDurationMs) + "ms");

    if (onComplete) onComplete(finalCount);
}

void MemoryScanner::DoNextScan(ScanValue targetValue, std::function<void(size_t)> onComplete) {
    LOG_INFO("Starting Next Scan (Filter) for value: " + targetValue.ToString());
    auto startTime = std::chrono::high_resolution_clock::now();

    std::vector<CandidateAddress> currentCandidates;
    {
        std::lock_guard<std::mutex> lock(m_candidatesMutex);
        currentCandidates = m_candidates;
    }

    std::vector<CandidateAddress> survivingCandidates;
    survivingCandidates.reserve(currentCandidates.size());

    size_t total = currentCandidates.size();
    size_t processed = 0;

    size_t valSize = 4;
    if (targetValue.type == ScanDataType::Int64 || targetValue.type == ScanDataType::Double) {
        valSize = 8;
    } else if (targetValue.type == ScanDataType::String) {
        valSize = std::max((size_t)1, targetValue.stringVal.size());
    }

    std::vector<uint8_t> buffer(std::max(valSize, (size_t)8), 0);

    for (auto& cand : currentCandidates) {
        if (m_cancelRequested) break;

        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(cand.address), buffer.data(), valSize, &bytesRead) && bytesRead == valSize) {
            if (targetValue.Matches(buffer.data(), 0)) {
                cand.previousValueInt = cand.currentValueInt;
                cand.previousValueDouble = cand.currentValueDouble;
                cand.previousValueString = cand.currentValueString;
                cand.currentValueInt = targetValue.intVal;
                cand.currentValueDouble = targetValue.doubleVal;
                cand.currentValueString = targetValue.stringVal;
                survivingCandidates.push_back(cand);
            }
        }

        processed++;
        if (total > 0 && (processed % 1000 == 0 || processed == total)) {
            m_progress = static_cast<float>(processed) / static_cast<float>(total);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_candidatesMutex);
        m_candidates = std::move(survivingCandidates);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double scanDurationMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    size_t finalCount = GetCandidateCount();
    m_progress = 1.0f;
    m_isScanning = false;

    LOG_SUCCESS("Next scan complete: narrowed to " + StringUtils::FormatNumberWithCommas(finalCount) + 
                " candidates in " + std::to_string((int)scanDurationMs) + "ms");

    if (onComplete) onComplete(finalCount);
}

void MemoryScanner::RefreshCandidateValues() {
    if (!m_hProcess || m_isScanning) return;

    std::lock_guard<std::mutex> lock(m_candidatesMutex);
    size_t valSize = (m_currentDataType == ScanDataType::Int64 || m_currentDataType == ScanDataType::Double) ? 8 : 4;
    if (m_currentDataType == ScanDataType::String) {
        valSize = 64;
    }
    std::vector<uint8_t> buffer(valSize, 0);

    for (auto& cand : m_candidates) {
        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(cand.address), buffer.data(), valSize, &bytesRead) && bytesRead > 0) {
            cand.previousValueInt = cand.currentValueInt;
            cand.previousValueDouble = cand.currentValueDouble;
            cand.previousValueString = cand.currentValueString;
            if (m_currentDataType == ScanDataType::Int32 && bytesRead >= 4) {
                cand.currentValueInt = *reinterpret_cast<int32_t*>(buffer.data());
            } else if (m_currentDataType == ScanDataType::Int64 && bytesRead >= 8) {
                cand.currentValueInt = *reinterpret_cast<int64_t*>(buffer.data());
            } else if (m_currentDataType == ScanDataType::Float && bytesRead >= 4) {
                cand.currentValueDouble = *reinterpret_cast<float*>(buffer.data());
            } else if (m_currentDataType == ScanDataType::Double && bytesRead >= 8) {
                cand.currentValueDouble = *reinterpret_cast<double*>(buffer.data());
            } else if (m_currentDataType == ScanDataType::String) {
                std::string s;
                for (size_t b = 0; b < bytesRead; ++b) {
                    char c = static_cast<char>(buffer[b]);
                    if (c == '\0') break;
                    s.push_back(c);
                }
                cand.currentValueString = s;
            }
        }
    }
}

bool MemoryScanner::WriteValue(uintptr_t address, const ScanValue& value) {
    if (!m_hProcess) return false;

    size_t valSize = 4;
    std::vector<uint8_t> buffer;

    switch (value.type) {
    case ScanDataType::Int32: {
        valSize = 4;
        buffer.resize(4);
        int32_t v = static_cast<int32_t>(value.intVal);
        memcpy(buffer.data(), &v, 4);
        break;
    }
    case ScanDataType::Int64: {
        valSize = 8;
        buffer.resize(8);
        int64_t v = value.intVal;
        memcpy(buffer.data(), &v, 8);
        break;
    }
    case ScanDataType::Float: {
        valSize = 4;
        buffer.resize(4);
        float v = static_cast<float>(value.doubleVal);
        memcpy(buffer.data(), &v, 4);
        break;
    }
    case ScanDataType::Double: {
        valSize = 8;
        buffer.resize(8);
        double v = value.doubleVal;
        memcpy(buffer.data(), &v, 8);
        break;
    }
    case ScanDataType::String: {
        valSize = value.stringVal.size() + 1; // Include null-terminator for string writes
        buffer.resize(valSize);
        memcpy(buffer.data(), value.stringVal.c_str(), valSize);
        break;
    }
    }

    SIZE_T bytesWritten = 0;
    BOOL res = WriteProcessMemory(m_hProcess, reinterpret_cast<LPVOID>(address), buffer.data(), valSize, &bytesWritten);
    if (res && bytesWritten == valSize) {
        LOG_SUCCESS("Wrote " + value.ToString() + " to " + StringUtils::FormatAddress(address));
        {
            std::lock_guard<std::mutex> guard(m_candidatesMutex);
            for (auto& cand : m_candidates) {
                if (cand.address == address) {
                    cand.previousValueInt = cand.currentValueInt;
                    cand.previousValueDouble = cand.currentValueDouble;
                    cand.previousValueString = cand.currentValueString;
                    cand.currentValueInt = value.intVal;
                    cand.currentValueDouble = value.doubleVal;
                    cand.currentValueString = value.stringVal;
                    if (cand.isLocked) {
                        cand.lockValueInt = value.intVal;
                        cand.lockValueDouble = value.doubleVal;
                        cand.lockValueString = value.stringVal;
                    }
                    break;
                }
            }
        }
        return true;
    }
    LOG_ERROR("Failed to write to " + StringUtils::FormatAddress(address) + " (Error: " + std::to_string(GetLastError()) + ")");
    return false;
}

bool MemoryScanner::ReadValue(uintptr_t address, ScanDataType type, ScanValue& outVal) {
    if (!m_hProcess) return false;

    outVal.type = type;
    size_t valSize = (type == ScanDataType::Int64 || type == ScanDataType::Double) ? 8 : 4;
    if (type == ScanDataType::String) {
        valSize = 64;
    }
    std::vector<uint8_t> buffer(valSize, 0);

    SIZE_T bytesRead = 0;
    if (ReadProcessMemory(m_hProcess, reinterpret_cast<LPCVOID>(address), buffer.data(), valSize, &bytesRead) && bytesRead > 0) {
        switch (type) {
        case ScanDataType::Int32:
            if (bytesRead < 4) return false;
            outVal.intVal = *reinterpret_cast<int32_t*>(buffer.data());
            outVal.doubleVal = static_cast<double>(outVal.intVal);
            outVal.stringVal = std::to_string(outVal.intVal);
            break;
        case ScanDataType::Int64:
            if (bytesRead < 8) return false;
            outVal.intVal = *reinterpret_cast<int64_t*>(buffer.data());
            outVal.doubleVal = static_cast<double>(outVal.intVal);
            outVal.stringVal = std::to_string(outVal.intVal);
            break;
        case ScanDataType::Float:
            if (bytesRead < 4) return false;
            outVal.doubleVal = *reinterpret_cast<float*>(buffer.data());
            outVal.intVal = static_cast<int64_t>(outVal.doubleVal);
            outVal.stringVal = std::to_string(static_cast<float>(outVal.doubleVal));
            break;
        case ScanDataType::Double:
            if (bytesRead < 8) return false;
            outVal.doubleVal = *reinterpret_cast<double*>(buffer.data());
            outVal.intVal = static_cast<int64_t>(outVal.doubleVal);
            outVal.stringVal = std::to_string(outVal.doubleVal);
            break;
        case ScanDataType::String: {
            std::string s;
            for (size_t b = 0; b < bytesRead; ++b) {
                char c = static_cast<char>(buffer[b]);
                if (c == '\0') break;
                s.push_back(c);
            }
            outVal.stringVal = s;
            break;
        }
        }
        return true;
    }
    return false;
}

void MemoryScanner::SetAddressLock(uintptr_t address, bool lock, const ScanValue& val) {
    std::lock_guard<std::mutex> guard(m_candidatesMutex);
    for (auto& cand : m_candidates) {
        if (cand.address == address) {
            cand.isLocked = lock;
            cand.lockValueInt = val.intVal;
            cand.lockValueDouble = val.doubleVal;
            cand.lockValueString = val.stringVal;
            break;
        }
    }
}

void MemoryScanner::LockThreadLoop() {
    while (m_lockThreadRunning) {
        if (m_hProcess && !m_isScanning) {
            std::vector<CandidateAddress> lockedCands;
            {
                std::lock_guard<std::mutex> guard(m_candidatesMutex);
                for (const auto& cand : m_candidates) {
                    if (cand.isLocked) lockedCands.push_back(cand);
                }
            }

            for (const auto& cand : lockedCands) {
                ScanValue val;
                val.type = m_currentDataType;
                val.intVal = cand.lockValueInt;
                val.doubleVal = cand.lockValueDouble;
                val.stringVal = cand.lockValueString;
                WriteValue(cand.address, val);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}
