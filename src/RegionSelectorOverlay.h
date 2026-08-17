#pragma once
#include <windows.h>
#include <functional>
#include "ScreenCapture.h"

class RegionSelectorOverlay {
public:
    static RegionSelectorOverlay& Get() {
        static RegionSelectorOverlay instance;
        return instance;
    }

    void StartSelection(std::function<void(const CaptureRegion&)> onSelected);
    bool IsSelecting() const { return m_isSelecting; }

private:
    RegionSelectorOverlay();
    ~RegionSelectorOverlay();

    bool m_isSelecting = false;
    HWND m_hwndOverlay = nullptr;
    POINT m_startPoint = { 0, 0 };
    POINT m_currentPoint = { 0, 0 };
    bool m_isDragging = false;
    std::function<void(const CaptureRegion&)> m_onSelected;

    static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void RegisterOverlayClass();
};
