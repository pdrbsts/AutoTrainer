#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <mutex>

struct TrainerOcrResult {
    std::string fullText;
    std::string filteredNumberText;
    int64_t parsedInt64 = 0;
    double parsedDouble = 0.0;
    bool isValidNumber = false;
    float confidence = 1.0f;
    double recognitionTimeMs = 0.0;
};

class AppOcrEngine {
public:
    AppOcrEngine();
    ~AppOcrEngine();

    bool Initialize();
    bool IsAvailable() const { return m_isAvailable; }
    std::string GetLanguageName() const { return m_languageName; }

    // Performs OCR on BGRA raw pixels
    TrainerOcrResult Recognize(const std::vector<uint8_t>& bgraPixels, int width, int height);

private:
    bool m_isAvailable = false;
    std::string m_languageName = "Default";
    std::mutex m_ocrMutex;

    // Helper for extracting clean numbers from OCR output string
    TrainerOcrResult ParseOcrOutput(const std::string& rawText);

    // Fallback digit recognizer for stripped Windows versions without WinRT language pack
    TrainerOcrResult FallbackDigitRecognizer(const std::vector<uint8_t>& bgraPixels, int width, int height);
};
