#include <windows.h>
#include <string>
#include <sstream>

// Global game variables (stored in process memory)
volatile int32_t g_playerAura = 1500;
volatile int32_t g_playerGold = 8200;
volatile int32_t g_playerHealth = 100;
char g_playerRank[32] = "GrandMaster";
bool g_autoMode = false;

const wchar_t* WINDOW_CLASS_NAME = L"MockGameWindowClass";

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);

        // Dark RPG game background
        HBRUSH bgBrush = CreateSolidBrush(RGB(20, 24, 32));
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);

        SetBkMode(hdc, TRANSPARENT);

        // Header font
        HFONT hTitleFont = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hTitleFont);

        SetTextColor(hdc, RGB(180, 200, 220));
        TextOutW(hdc, 20, 20, L"🎮 MOCK RPG GAME (AutoTrainer Test Target)", 42);

        // Subtitle
        HFONT hSubFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Segoe UI");
        SelectObject(hdc, hSubFont);
        SetTextColor(hdc, RGB(120, 140, 160));
        TextOutW(hdc, 20, 50, L"Controls: [Space] Aura +250 | [R] Cycle Rank | [Backspace] Aura -100 | [A] Auto | [Esc] Exit", 92);

        // Aura Display Box
        RECT auraBox = { 30, 90, 450, 200 };
        HBRUSH boxBrush = CreateSolidBrush(RGB(30, 36, 48));
        FillRect(hdc, &auraBox, boxBrush);
        DeleteObject(boxBrush);

        HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(0, 180, 220));
        SelectObject(hdc, borderPen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, auraBox.left, auraBox.top, auraBox.right, auraBox.bottom);
        DeleteObject(borderPen);

        // Label
        SetTextColor(hdc, RGB(0, 210, 255));
        TextOutW(hdc, 50, 105, L"AURA BALANCE:", 13);

        // Large crisp digital numbers for OCR
        HFONT hNumberFont = CreateFontW(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, L"Arial");
        SelectObject(hdc, hNumberFont);

        std::wstring auraStr = std::to_wstring(g_playerAura);
        SetTextColor(hdc, RGB(255, 255, 255));
        TextOutW(hdc, 50, 130, auraStr.c_str(), (int)auraStr.length());

        // Secondary stats
        SelectObject(hdc, hSubFont);
        SetTextColor(hdc, RGB(160, 170, 180));
        wchar_t wRank[64] = { 0 };
        MultiByteToWideChar(CP_UTF8, 0, g_playerRank, -1, wRank, 64);
        std::wstring statsStr = L"Gold: " + std::to_wstring(g_playerGold) + L"   |   Health: " + std::to_wstring(g_playerHealth) + L"   |   Rank: " + wRank;
        TextOutW(hdc, 30, 220, statsStr.c_str(), (int)statsStr.length());

        std::wstring statusStr = g_autoMode ? L"Status: Auto-changing aura every 2 seconds..." : L"Status: Manual (Press Space to change aura, R to change rank)";
        SetTextColor(hdc, g_autoMode ? RGB(100, 255, 140) : RGB(200, 200, 200));
        TextOutW(hdc, 30, 250, statusStr.c_str(), (int)statusStr.length());

        SelectObject(hdc, hOldFont);
        DeleteObject(hTitleFont);
        DeleteObject(hSubFont);
        DeleteObject(hNumberFont);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_SPACE) {
            g_playerAura += 250;
            InvalidateRect(hwnd, NULL, TRUE);
        } else if (wParam == 'R' || wParam == 'r') {
            static int rankIdx = 0;
            const char* ranks[] = { "GrandMaster", "ShadowKnight", "ArchMage", "Paladin", "Champion" };
            rankIdx = (rankIdx + 1) % 5;
            strncpy_s(g_playerRank, sizeof(g_playerRank), ranks[rankIdx], _TRUNCATE);
            InvalidateRect(hwnd, NULL, TRUE);
        } else if (wParam == VK_BACK) {
            if (g_playerAura >= 100) g_playerAura -= 100;
            InvalidateRect(hwnd, NULL, TRUE);
        } else if (wParam == 'A' || wParam == 'a') {
            g_autoMode = !g_autoMode;
            if (g_autoMode) {
                SetTimer(hwnd, 1, 2000, NULL);
            } else {
                KillTimer(hwnd, 1);
            }
            InvalidateRect(hwnd, NULL, TRUE);
        } else if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        return 0;
    }
    case WM_TIMER: {
        if (wParam == 1) {
            g_playerAura += 150;
            if (g_playerAura > 20000) g_playerAura = 1000;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(
        WINDOW_CLASS_NAME,
        L"RPG Test Game - AutoTrainer Target",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        150, 150, 520, 340,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Background thread to poll memory changes so the UI reflects writes from AutoTrainer
    SetTimer(hwnd, 2, 100, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_TIMER && msg.wParam == 2) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
