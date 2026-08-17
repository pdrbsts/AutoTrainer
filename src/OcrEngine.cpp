#include "OcrEngine.h"
#include "Utils.h"
#include <chrono>
#include <sstream>
#include <cctype>

// WinRT Headers
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Globalization.h>

static winrt::Windows::Media::Ocr::OcrEngine g_winrtEngine = nullptr;

AppOcrEngine::AppOcrEngine() {
}

AppOcrEngine::~AppOcrEngine() {
}

bool AppOcrEngine::Initialize() {
    try {
        // Try user profile languages first
        if (winrt::Windows::Media::Ocr::OcrEngine::IsLanguageSupported(winrt::Windows::Globalization::Language(L"en-US"))) {
            g_winrtEngine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromLanguage(winrt::Windows::Globalization::Language(L"en-US"));
            m_languageName = "en-US";
        }
        
        if (!g_winrtEngine) {
            g_winrtEngine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromUserProfileLanguages();
            if (g_winrtEngine) {
                m_languageName = StringUtils::WideToUtf8(g_winrtEngine.RecognizerLanguage().LanguageTag().c_str());
            }
        }

        if (!g_winrtEngine) {
            auto langs = winrt::Windows::Media::Ocr::OcrEngine::AvailableRecognizerLanguages();
            if (langs.Size() > 0) {
                g_winrtEngine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromLanguage(langs.GetAt(0));
                m_languageName = StringUtils::WideToUtf8(langs.GetAt(0).LanguageTag().c_str());
            }
        }

        if (g_winrtEngine) {
            m_isAvailable = true;
            LOG_SUCCESS("Windows Media OCR initialized successfully (Language: " + m_languageName + ")");
            return true;
        }
    } catch (const winrt::hresult_error& ex) {
        LOG_WARNING("WinRT OCR init exception: " + StringUtils::WideToUtf8(ex.message().c_str()) + " - Using fallback digit OCR.");
    } catch (...) {
        LOG_WARNING("WinRT OCR unavailable - Using fallback digit OCR.");
    }

    m_isAvailable = true;
    m_languageName = "Native Fallback Digit OCR";
    LOG_INFO("Initialized with Native Fallback Digit OCR");
    return true;
}

TrainerOcrResult AppOcrEngine::Recognize(const std::vector<uint8_t>& bgraPixels, int width, int height) {
    if (bgraPixels.empty() || width <= 0 || height <= 0) {
        return TrainerOcrResult();
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(m_ocrMutex);

    if (g_winrtEngine) {
        try {
            // Create DataWriter to convert raw BGRA vector into WinRT IBuffer
            winrt::Windows::Storage::Streams::DataWriter writer;
            writer.WriteBytes(winrt::array_view<const uint8_t>(bgraPixels.data(), bgraPixels.data() + bgraPixels.size()));
            winrt::Windows::Storage::Streams::IBuffer buffer = writer.DetachBuffer();

            // Create SoftwareBitmap
            winrt::Windows::Graphics::Imaging::SoftwareBitmap bitmap = winrt::Windows::Graphics::Imaging::SoftwareBitmap::CreateCopyFromBuffer(
                buffer,
                winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
                width,
                height,
                winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Ignore
            );

            // Run OCR
            auto asyncOp = g_winrtEngine.RecognizeAsync(bitmap);
            auto result = asyncOp.get();

            std::wstring recognizedTextW = result.Text().c_str();
            std::string recognizedText = StringUtils::WideToUtf8(recognizedTextW);

            TrainerOcrResult out = ParseOcrOutput(recognizedText);

            auto endTime = std::chrono::high_resolution_clock::now();
            out.recognitionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
            return out;
        } catch (...) {
            // Fall back to lightweight digit recognizer if WinRT call fails
        }
    }

    auto out = FallbackDigitRecognizer(bgraPixels, width, height);
    auto endTime = std::chrono::high_resolution_clock::now();
    out.recognitionTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    return out;
}

TrainerOcrResult AppOcrEngine::ParseOcrOutput(const std::string& rawText) {
    TrainerOcrResult res;
    res.fullText = StringUtils::Trim(rawText);

    // Extract numbers, handles formats like "1,234", "1000", "50.5", "-420"
    std::string numStr;
    bool hasDot = false;

    for (size_t i = 0; i < res.fullText.size(); ++i) {
        char c = res.fullText[i];
        if (isdigit((unsigned char)c)) {
            numStr += c;
        } else if (c == '-' && numStr.empty()) {
            numStr += c;
        } else if ((c == '.' || c == ',') && !hasDot) {
            bool isThousandsSep = false;
            if (i + 3 < res.fullText.size() && isdigit((unsigned char)res.fullText[i+1]) && 
                isdigit((unsigned char)res.fullText[i+2]) && isdigit((unsigned char)res.fullText[i+3])) {
                if (i + 4 >= res.fullText.size() || !isdigit((unsigned char)res.fullText[i+4])) {
                    isThousandsSep = true;
                }
            }
            
            if (isThousandsSep) {
                continue;
            } else {
                numStr += '.';
                hasDot = true;
            }
        }
    }

    res.filteredNumberText = numStr;

    if (!numStr.empty() && numStr != "-") {
        if (StringUtils::ParseInt64(numStr, res.parsedInt64)) {
            res.isValidNumber = true;
        }
        StringUtils::ParseDouble(numStr, res.parsedDouble);
    }

    return res;
}

TrainerOcrResult AppOcrEngine::FallbackDigitRecognizer(const std::vector<uint8_t>& bgraPixels, int width, int height) {
    TrainerOcrResult res;
    
    // Convert to binary image
    std::vector<uint8_t> binary(width * height, 0);
    uint32_t sumBrightness = 0;
    for (int i = 0; i < width * height; ++i) {
        int idx = i * 4;
        uint8_t b = bgraPixels[idx];
        uint8_t g = bgraPixels[idx + 1];
        uint8_t r = bgraPixels[idx + 2];
        uint8_t gray = (uint8_t)(0.299f * r + 0.587f * g + 0.114f * b);
        sumBrightness += gray;
    }
    uint8_t avgThresh = (uint8_t)(sumBrightness / (width * height));
    int brightCount = 0;
    for (int i = 0; i < width * height; ++i) {
        int idx = i * 4;
        uint8_t gray = (uint8_t)(0.299f * bgraPixels[idx+2] + 0.587f * bgraPixels[idx+1] + 0.114f * bgraPixels[idx]);
        if (gray > avgThresh) brightCount++;
    }
    bool inverted = (brightCount > (width * height) / 2);

    for (int i = 0; i < width * height; ++i) {
        int idx = i * 4;
        uint8_t gray = (uint8_t)(0.299f * bgraPixels[idx+2] + 0.587f * bgraPixels[idx+1] + 0.114f * bgraPixels[idx]);
        bool isFg = inverted ? (gray < avgThresh - 20) : (gray > avgThresh + 20);
        binary[i] = isFg ? 1 : 0;
    }

    // Column projection to find bounding boxes of characters
    std::vector<int> colProj(width, 0);
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            colProj[x] += binary[y * width + x];
        }
    }

    struct CharBox { int minX, maxX, minY, maxY; };
    std::vector<CharBox> chars;
    bool inChar = false;
    int startX = 0;

    for (int x = 0; x < width; ++x) {
        if (colProj[x] > 1) {
            if (!inChar) {
                inChar = true;
                startX = x;
            }
        } else {
            if (inChar) {
                inChar = false;
                if (x - startX >= 2) {
                    int minY = height, maxY = 0;
                    for (int cy = 0; cy < height; ++cy) {
                        for (int cx = startX; cx <= x; ++cx) {
                            if (binary[cy * width + cx]) {
                                if (cy < minY) minY = cy;
                                if (cy > maxY) maxY = cy;
                            }
                        }
                    }
                    if (maxY - minY >= 4) {
                        chars.push_back({ startX, x, minY, maxY });
                    }
                }
            }
        }
    }

    std::string digits;
    for (const auto& box : chars) {
        int bw = box.maxX - box.minX + 1;
        int bh = box.maxY - box.minY + 1;
        if (bw <= 0 || bh <= 0) continue;

        int grid[7][5] = {0};
        for (int gy = 0; gy < 7; ++gy) {
            for (int gx = 0; gx < 5; ++gx) {
                int px = box.minX + (gx * bw) / 5;
                int py = box.minY + (gy * bh) / 7;
                grid[gy][gx] = binary[py * width + px];
            }
        }

        char detected = '?';
        float aspect = (float)bw / (float)bh;
        if (aspect < 0.35f) {
            detected = '1';
        } else if (grid[0][0] && grid[0][4] && grid[6][0] && grid[6][4] && !grid[3][2]) {
            detected = '0';
        } else if (grid[3][0] && grid[3][1] && grid[3][2] && grid[3][3] && grid[3][4]) {
            if (grid[1][0] && !grid[1][4]) detected = '5';
            else if (!grid[1][0] && grid[1][4] && grid[5][0] && !grid[5][4]) detected = '2';
            else if (!grid[1][0] && grid[1][4] && !grid[5][0] && grid[5][4]) detected = '3';
            else if (grid[1][0] && grid[1][4] && grid[5][0] && grid[5][4]) detected = '8';
            else detected = '8';
        } else if (grid[1][0] && grid[5][0] && grid[5][4] && grid[0][2]) {
            detected = '6';
        } else if (grid[1][0] && grid[1][4] && grid[5][4] && grid[0][2]) {
            detected = '9';
        } else if (grid[1][0] && grid[1][4] && grid[3][4] && !grid[5][0]) {
            detected = '4';
        } else if (grid[0][0] && grid[0][4] && grid[6][0]) {
            detected = '7';
        }

        if (detected != '?') {
            digits += detected;
        }
    }

    res.fullText = digits;
    res.filteredNumberText = digits;
    if (!digits.empty()) {
        StringUtils::ParseInt64(digits, res.parsedInt64);
        res.parsedDouble = (double)res.parsedInt64;
        res.isValidNumber = true;
    }
    return res;
}
