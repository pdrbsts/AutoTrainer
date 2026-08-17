#include "TrainerController.h"
#include "Utils.h"

TrainerController::TrainerController(ProcessManager& procMgr, ScreenCapture& capture, AppOcrEngine& ocr, MemoryScanner& scanner)
    : m_procMgr(procMgr), m_capture(capture), m_ocr(ocr), m_scanner(scanner) {
    m_workerThread = std::thread(&TrainerController::WorkerLoop, this);
}

TrainerController::~TrainerController() {
    m_autoTrainerActive = false;
    m_workerRunning = false;
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void TrainerController::StartAutoTrainer() {
    if (!m_procMgr.IsAttached()) {
        LOG_WARNING("Cannot start AutoTrainer: No target process attached.");
        return;
    }
    m_autoTrainerActive = true;
    if (m_scanner.GetCandidateCount() == 0) {
        m_state = TrainerState::Ready;
    } else if (m_scanner.GetCandidateCount() <= targetCandidateThreshold) {
        m_state = TrainerState::TargetLocked;
    } else {
        m_state = TrainerState::WaitingForValueChange;
    }
    LOG_INFO("AutoTrainer started in continuous auto-scan mode.");
}

void TrainerController::StopAutoTrainer() {
    m_autoTrainerActive = false;
    LOG_INFO("AutoTrainer paused.");
}

void TrainerController::Reset() {
    m_autoTrainerActive = false;
    m_scanner.Reset();
    m_state = m_procMgr.IsAttached() ? TrainerState::Ready : TrainerState::Idle;
    m_consecutiveStableFrames = 0;
    m_lastStableOcrString = "";
    LOG_INFO("AutoTrainer reset to initial state.");
}

void TrainerController::TriggerManualFirstScan() {
    TrainerOcrResult ocr;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        ocr = m_lastOcrResult;
    }

    ScanDataType currentDt = m_scanner.GetCurrentDataType();
    ScanValue val;
    val.type = currentDt;

    if (currentDt == ScanDataType::String) {
        std::string textToScan = !ocr.fullText.empty() ? ocr.fullText : ocr.filteredNumberText;
        if (textToScan.empty()) {
            LOG_WARNING("Manual First Scan failed: OCR has not detected any text yet.");
            return;
        }
        val.stringVal = textToScan;
    } else {
        if (!ocr.isValidNumber) {
            LOG_WARNING("Manual First Scan failed: OCR has not detected a valid number yet.");
            return;
        }
        val.intVal = ocr.parsedInt64;
        val.doubleVal = ocr.parsedDouble;
    }

    m_lastScannedValue = val;

    m_state = TrainerState::FirstScanning;
    m_scanner.StartFirstScan(val, [this](size_t count) {
        if (count <= targetCandidateThreshold && count > 0) {
            m_state = TrainerState::TargetLocked;
            LOG_SUCCESS("Target locked! Found exact " + std::to_string(count) + " memory addresses.");
        } else if (count > 0) {
            m_state = TrainerState::WaitingForValueChange;
            LOG_INFO("First scan finished (" + std::to_string(count) + " candidates). Modify target in game to filter.");
        } else {
            m_state = TrainerState::Ready;
            LOG_WARNING("First scan found 0 results for value " + m_lastScannedValue.ToString());
        }
    });
}

void TrainerController::TriggerManualNextScan() {
    TrainerOcrResult ocr;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);
        ocr = m_lastOcrResult;
    }

    ScanDataType currentDt = m_scanner.GetCurrentDataType();
    ScanValue val;
    val.type = currentDt;

    if (currentDt == ScanDataType::String) {
        std::string textToScan = !ocr.fullText.empty() ? ocr.fullText : ocr.filteredNumberText;
        if (textToScan.empty()) {
            LOG_WARNING("Manual Next Scan failed: OCR has not detected any text yet.");
            return;
        }
        val.stringVal = textToScan;
    } else {
        if (!ocr.isValidNumber) {
            LOG_WARNING("Manual Next Scan failed: OCR has not detected a valid number yet.");
            return;
        }
        val.intVal = ocr.parsedInt64;
        val.doubleVal = ocr.parsedDouble;
    }

    m_lastScannedValue = val;

    m_state = TrainerState::FilterScanning;
    m_scanner.StartNextScan(val, [this](size_t count) {
        if (count <= targetCandidateThreshold && count > 0) {
            m_state = TrainerState::TargetLocked;
            LOG_SUCCESS("Target locked! Filtered down to " + std::to_string(count) + " address(es)!");
        } else if (count > 0) {
            m_state = TrainerState::WaitingForValueChange;
            LOG_INFO("Filtered to " + std::to_string(count) + " candidates. Waiting for next value change...");
        } else {
            m_state = TrainerState::Ready;
            LOG_WARNING("Next scan resulted in 0 candidates.");
        }
    });
}

std::string TrainerController::GetStateString() const {
    switch (m_state) {
    case TrainerState::Idle: return "Idle (Attach to Process)";
    case TrainerState::Ready: return "Ready for 1st Scan";
    case TrainerState::FirstScanning: return "Performing First Memory Scan...";
    case TrainerState::WaitingForValueChange: return "Waiting for Aura to change in game...";
    case TrainerState::FilterScanning: return "Filtering Candidates (Next Scan)...";
    case TrainerState::TargetLocked: return "Target Address Locked!";
    }
    return "Unknown";
}

TrainerOcrResult TrainerController::GetLastOcrResult() const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    return m_lastOcrResult;
}

void TrainerController::WorkerLoop() {
    while (m_workerRunning) {
        if (m_procMgr.IsAttached()) {
            // 1. Capture screen region
            int w = 0, h = 0;
            m_capture.Capture(m_procMgr.GetTargetHwnd());
            std::vector<uint8_t> pixels = m_capture.GetPixelsBGRA(w, h);

            // 2. Perform OCR
            if (!pixels.empty() && w > 0 && h > 0) {
                TrainerOcrResult ocr = m_ocr.Recognize(pixels, w, h);
                {
                    std::lock_guard<std::mutex> lock(m_stateMutex);
                    m_lastOcrResult = ocr;
                }

                // 3. Process AutoTrainer state machine
                if (m_autoTrainerActive) {
                    ProcessOcrFrame(ocr);
                }
            }

            // 4. Update candidate live values periodically
            if (m_scanner.GetCandidateCount() > 0 && !m_scanner.IsScanning()) {
                m_scanner.RefreshCandidateValues();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(captureIntervalMs));
    }
}

void TrainerController::ProcessOcrFrame(const TrainerOcrResult& ocr) {
    if (m_scanner.IsScanning()) {
        return;
    }

    ScanDataType currentDt = m_scanner.GetCurrentDataType();
    std::string frameText = (currentDt == ScanDataType::String) ? ocr.fullText : ocr.filteredNumberText;

    if (currentDt != ScanDataType::String && !ocr.isValidNumber) {
        return;
    }
    if (currentDt == ScanDataType::String && frameText.empty()) {
        return;
    }

    // Check stability (must see same value for N frames to avoid reading mid-animation numbers/text)
    if (frameText == m_lastStableOcrString) {
        m_consecutiveStableFrames++;
    } else {
        m_lastStableOcrString = frameText;
        m_consecutiveStableFrames = 1;
        return;
    }

    if (m_consecutiveStableFrames < stabilityFramesRequired) {
        return;
    }

    ScanValue currentVal;
    currentVal.type = currentDt;
    if (currentDt == ScanDataType::String) {
        currentVal.stringVal = frameText;
    } else {
        currentVal.intVal = ocr.parsedInt64;
        currentVal.doubleVal = ocr.parsedDouble;
    }

    // Check state transitions
    if (m_state == TrainerState::Ready && autoStartFirstScan) {
        LOG_INFO("AutoTrainer: Triggering initial First Scan for OCR value: " + currentVal.ToString());
        m_lastScannedValue = currentVal;
        m_state = TrainerState::FirstScanning;

        m_scanner.StartFirstScan(currentVal, [this](size_t count) {
            if (count <= targetCandidateThreshold && count > 0) {
                m_state = TrainerState::TargetLocked;
                LOG_SUCCESS("Target locked on first scan! (" + std::to_string(count) + " candidates)");
            } else if (count > 0) {
                m_state = TrainerState::WaitingForValueChange;
                LOG_INFO("First scan complete (" + std::to_string(count) + " candidates). Now change value in game!");
            } else {
                m_state = TrainerState::Ready;
                LOG_WARNING("First scan found 0 candidates.");
            }
        });
    }
    else if (m_state == TrainerState::WaitingForValueChange) {
        bool valueChanged = false;
        if (currentVal.type == ScanDataType::String) {
            valueChanged = (currentVal.stringVal != m_lastScannedValue.stringVal);
        } else if (currentVal.type == ScanDataType::Int32 || currentVal.type == ScanDataType::Int64) {
            valueChanged = (currentVal.intVal != m_lastScannedValue.intVal);
        } else {
            valueChanged = (std::fabs(currentVal.doubleVal - m_lastScannedValue.doubleVal) > 0.001);
        }

        if (valueChanged) {
            LOG_INFO("AutoTrainer: Detected value change from " + m_lastScannedValue.ToString() + 
                     " -> " + currentVal.ToString() + ". Auto-filtering candidates...");
            
            m_lastScannedValue = currentVal;
            m_state = TrainerState::FilterScanning;

            m_scanner.StartNextScan(currentVal, [this](size_t count) {
                if (count <= targetCandidateThreshold && count > 0) {
                    m_state = TrainerState::TargetLocked;
                    LOG_SUCCESS("Target narrowed down to " + std::to_string(count) + " candidate(s)! Address locked.");
                } else if (count > 0) {
                    m_state = TrainerState::WaitingForValueChange;
                    LOG_INFO("Filtered to " + std::to_string(count) + " candidates. Waiting for next value change in game...");
                } else {
                    m_state = TrainerState::Ready;
                    LOG_WARNING("Next scan resulted in 0 candidates. Please reset.");
                }
            });
        }
    }
}
