#pragma once
#include <windows.h>
#include <d3d11.h>
#include <vector>
#include <mutex>
#include <cstdint>

struct CaptureRegion {
    int x = 100;
    int y = 100;
    int width = 200;
    int height = 60;
    bool relativeToWindow = false;
};

class ScreenCapture {
public:
    ScreenCapture();
    ~ScreenCapture();

    void SetRegion(const CaptureRegion& region);
    CaptureRegion GetRegion() const;

    // Captures the region and returns true if successful
    bool Capture(HWND targetHwnd = nullptr);

    // Get raw BGRA pixels (width * height * 4 bytes)
    std::vector<uint8_t> GetPixelsBGRA(int& outWidth, int& outHeight);

    // Updates or creates a DirectX 11 ShaderResourceView for ImGui preview
    bool UpdateDx11Texture(ID3D11Device* device, ID3D11ShaderResourceView** outSrv, int& outWidth, int& outHeight);

    void ReleaseTexture();

    // Image pre-processing options
    bool applyGrayscale = false;
    bool applyThreshold = false;
    int thresholdValue = 128;
    bool invertColors = false;

private:
    CaptureRegion m_region;
    mutable std::mutex m_mutex;
    std::vector<uint8_t> m_pixels;
    int m_lastWidth = 0;
    int m_lastHeight = 0;

    // DX11 resources
    ID3D11Texture2D* m_previewTexture = nullptr;
    ID3D11ShaderResourceView* m_previewSRV = nullptr;
    int m_texWidth = 0;
    int m_texHeight = 0;

    void ProcessPixels(std::vector<uint8_t>& pixels, int width, int height);
};
