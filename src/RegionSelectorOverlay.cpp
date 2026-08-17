#include "RegionSelectorOverlay.h"
#include "Utils.h"
#include <algorithm>

static const wchar_t* OVERLAY_CLASS_NAME = L"AutoTrainerRegionOverlay";

RegionSelectorOverlay::RegionSelectorOverlay() {
    RegisterOverlayClass();
}

RegionSelectorOverlay::~RegionSelectorOverlay() {
    if (m_hwndOverlay && IsWindow(m_hwndOverlay)) {
        DestroyWindow(m_hwndOverlay);
    }
}

void RegionSelectorOverlay::RegisterOverlayClass() {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = OVERLAY_CLASS_NAME;
    RegisterClassExW(&wc);
}

void RegionSelectorOverlay::StartSelection(std::function<void(const CaptureRegion&)> onSelected) {
    if (m_isSelecting) return;

    m_onSelected = onSelected;
    m_isSelecting = true;
    m_isDragging = false;

    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    m_hwndOverlay = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        OVERLAY_CLASS_NAME,
        L"AutoTrainer Overlay",
        WS_POPUP,
        vx, vy, vw, vh,
        NULL, NULL, GetModuleHandle(NULL), this
    );

    if (m_hwndOverlay) {
        // Set semi-transparent dim effect (alpha 80 out of 255)
        SetLayeredWindowAttributes(m_hwndOverlay, 0, 80, LWA_ALPHA);
        ShowWindow(m_hwndOverlay, SW_SHOW);
        UpdateWindow(m_hwndOverlay);
        SetForegroundWindow(m_hwndOverlay);
        SetCapture(m_hwndOverlay);
    } else {
        m_isSelecting = false;
    }
}

LRESULT CALLBACK RegionSelectorOverlay::OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RegionSelectorOverlay* self = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        self = reinterpret_cast<RegionSelectorOverlay*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<RegionSelectorOverlay*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_LBUTTONDOWN: {
        self->m_isDragging = true;
        self->m_startPoint.x = (short)LOWORD(lParam);
        self->m_startPoint.y = (short)HIWORD(lParam);
        self->m_currentPoint = self->m_startPoint;
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (self->m_isDragging) {
            self->m_currentPoint.x = (short)LOWORD(lParam);
            self->m_currentPoint.y = (short)HIWORD(lParam);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (self->m_isDragging) {
            self->m_isDragging = false;
            ReleaseCapture();

            int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
            int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);

            int x1 = std::min(self->m_startPoint.x, self->m_currentPoint.x) + vx;
            int y1 = std::min(self->m_startPoint.y, self->m_currentPoint.y) + vy;
            int x2 = std::max(self->m_startPoint.x, self->m_currentPoint.x) + vx;
            int y2 = std::max(self->m_startPoint.y, self->m_currentPoint.y) + vy;

            CaptureRegion reg;
            reg.x = x1;
            reg.y = y1;
            reg.width = std::max(10, x2 - x1);
            reg.height = std::max(10, y2 - y1);
            reg.relativeToWindow = false;

            self->m_isSelecting = false;
            DestroyWindow(hwnd);
            self->m_hwndOverlay = nullptr;

            if (self->m_onSelected) {
                self->m_onSelected(reg);
            }
            LOG_INFO("Selected Region: X=" + std::to_string(reg.x) + " Y=" + std::to_string(reg.y) +
                     " W=" + std::to_string(reg.width) + " H=" + std::to_string(reg.height));
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            ReleaseCapture();
            self->m_isSelecting = false;
            self->m_isDragging = false;
            DestroyWindow(hwnd);
            self->m_hwndOverlay = nullptr;
            LOG_INFO("Region selection cancelled.");
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        if (self->m_isDragging) {
            int left = std::min(self->m_startPoint.x, self->m_currentPoint.x);
            int top = std::min(self->m_startPoint.y, self->m_currentPoint.y);
            int right = std::max(self->m_startPoint.x, self->m_currentPoint.x);
            int bottom = std::max(self->m_startPoint.y, self->m_currentPoint.y);

            // Draw bounding border
            HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 128));
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

            Rectangle(hdc, left, top, right, bottom);

            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(hPen);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
