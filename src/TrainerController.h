#pragma once
#include "ProcessManager.h"
#include "ScreenCapture.h"
#include "OcrEngine.h"
#include "MemoryScanner.h"
#include <atomic>
#include <thread>
#include <chrono>

enum class TrainerState {
    Idle,
    Ready,
    FirstScanning,
    WaitingForValueChange,
    FilterScanning,
    TargetLocked
};

class TrainerController {
public:
    TrainerController(ProcessManager& procMgr, ScreenCapture& capture, AppOcrEngine& ocr, MemoryScanner& scanner);
    ~TrainerController();

    void StartAutoTrainer();
    void StopAutoTrainer();
    bool IsAutoTrainerActive() const { return m_autoTrainerActive; }

    void TriggerManualFirstScan();
    void TriggerManualNextScan();
    void Reset();

    TrainerState GetState() const { return m_state; }
    std::string GetStateString() const;

    TrainerOcrResult GetLastOcrResult() const;
    ScanValue GetLastScannedValue() const { return m_lastScannedValue; }

    // Settings
    size_t targetCandidateThreshold = 3;
    int captureIntervalMs = 150;
    int stabilityFramesRequired = 2;
    bool autoStartFirstScan = true;

private:
    ProcessManager& m_procMgr;
    ScreenCapture& m_capture;
    AppOcrEngine& m_ocr;
    MemoryScanner& m_scanner;

    std::atomic<TrainerState> m_state{ TrainerState::Idle };
    std::atomic<bool> m_autoTrainerActive{ false };
    std::atomic<bool> m_workerRunning{ true };

    std::thread m_workerThread;

    mutable std::mutex m_stateMutex;
    TrainerOcrResult m_lastOcrResult;
    ScanValue m_lastScannedValue;
    int64_t m_candidateValue = 0;
    int m_consecutiveStableFrames = 0;
    std::string m_lastStableOcrString;

    void WorkerLoop();
    void ProcessOcrFrame(const TrainerOcrResult& ocr);
};
