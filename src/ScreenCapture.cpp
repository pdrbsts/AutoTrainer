#include "ScreenCapture.h"
#include "Utils.h"
#include <algorithm>

ScreenCapture::ScreenCapture() {
}

ScreenCapture::~ScreenCapture() {
    ReleaseTexture();
}

void ScreenCapture::SetRegion(const CaptureRegion& region) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_region = region;
    if (m_region.width <= 0) m_region.width = 10;
    if (m_region.height <= 0) m_region.height = 10;
}

CaptureRegion ScreenCapture::GetRegion() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_region;
}

void ScreenCapture::ReleaseTexture() {
    if (m_previewSRV) {
        m_previewSRV->Release();
        m_previewSRV = nullptr;
    }
    if (m_previewTexture) {
        m_previewTexture->Release();
        m_previewTexture = nullptr;
    }
    m_texWidth = 0;
    m_texHeight = 0;
}

bool ScreenCapture::Capture(HWND targetHwnd) {
    CaptureRegion reg;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        reg = m_region;
    }

    int srcX = reg.x;
    int srcY = reg.y;
    int width = reg.width;
    int height = reg.height;

    if (width <= 0 || height <= 0) return false;

    HDC hdcScreen = nullptr;
    RECT windowRect = { 0 };

    if (reg.relativeToWindow && targetHwnd && IsWindow(targetHwnd)) {
        GetWindowRect(targetHwnd, &windowRect);
        srcX += windowRect.left;
        srcY += windowRect.top;
        hdcScreen = GetDC(NULL); // Capture desktop at offset
    } else {
        hdcScreen = GetDC(NULL);
    }

    if (!hdcScreen) return false;

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    if (!hdcMem) {
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);

    if (!hBmp || !pBits) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    HGDIOBJ hOldBmp = SelectObject(hdcMem, hBmp);

    // Perform BitBlt capture
    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, srcX, srcY, SRCCOPY | CAPTUREBLT);

    // Copy to internal buffer
    std::vector<uint8_t> rawPixels(width * height * 4);
    memcpy(rawPixels.data(), pBits, width * height * 4);

    // Apply pre-processing
    ProcessPixels(rawPixels, width, height);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pixels = std::move(rawPixels);
        m_lastWidth = width;
        m_lastHeight = height;
    }

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return true;
}

void ScreenCapture::ProcessPixels(std::vector<uint8_t>& pixels, int width, int height) {
    if (!applyGrayscale && !applyThreshold && !invertColors) return;

    size_t count = width * height;
    for (size_t i = 0; i < count; ++i) {
        size_t idx = i * 4;
        uint8_t b = pixels[idx + 0];
        uint8_t g = pixels[idx + 1];
        uint8_t r = pixels[idx + 2];

        uint8_t gray = (uint8_t)(0.299f * r + 0.587f * g + 0.114f * b);

        if (applyThreshold) {
            gray = (gray >= thresholdValue) ? 255 : 0;
        }

        if (invertColors) {
            gray = 255 - gray;
            r = 255 - r;
            g = 255 - g;
            b = 255 - b;
        }

        if (applyGrayscale || applyThreshold) {
            pixels[idx + 0] = gray;
            pixels[idx + 1] = gray;
            pixels[idx + 2] = gray;
        } else if (invertColors) {
            pixels[idx + 0] = b;
            pixels[idx + 1] = g;
            pixels[idx + 2] = r;
        }
        pixels[idx + 3] = 255; // Alpha
    }
}

std::vector<uint8_t> ScreenCapture::GetPixelsBGRA(int& outWidth, int& outHeight) {
    std::lock_guard<std::mutex> lock(m_mutex);
    outWidth = m_lastWidth;
    outHeight = m_lastHeight;
    return m_pixels;
}

bool ScreenCapture::UpdateDx11Texture(ID3D11Device* device, ID3D11ShaderResourceView** outSrv, int& outWidth, int& outHeight) {
    if (!device) return false;

    std::vector<uint8_t> pixels;
    int width = 0, height = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pixels.empty() || m_lastWidth <= 0 || m_lastHeight <= 0) {
            return false;
        }
        pixels = m_pixels;
        width = m_lastWidth;
        height = m_lastHeight;
    }

    // Convert BGRA to RGBA for standard DX11 display
    for (size_t i = 0; i < pixels.size(); i += 4) {
        std::swap(pixels[i + 0], pixels[i + 2]); // B <-> R
        pixels[i + 3] = 255; // Full alpha
    }

    // Check if texture needs recreation
    if (!m_previewTexture || m_texWidth != width || m_texHeight != height) {
        ReleaseTexture();

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA subResource = {};
        subResource.pSysMem = pixels.data();
        subResource.SysMemPitch = width * 4;
        subResource.SysMemSlicePitch = 0;

        HRESULT hr = device->CreateTexture2D(&desc, &subResource, &m_previewTexture);
        if (FAILED(hr)) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;

        hr = device->CreateShaderResourceView(m_previewTexture, &srvDesc, &m_previewSRV);
        if (FAILED(hr)) {
            m_previewTexture->Release();
            m_previewTexture = nullptr;
            return false;
        }

        m_texWidth = width;
        m_texHeight = height;
    } else {
        // Update existing texture
        ID3D11DeviceContext* context = nullptr;
        device->GetImmediateContext(&context);
        if (context) {
            context->UpdateSubresource(m_previewTexture, 0, NULL, pixels.data(), width * 4, 0);
            context->Release();
        }
    }

    *outSrv = m_previewSRV;
    outWidth = width;
    outHeight = height;
    return true;
}
