#pragma once
#include <windows.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include "ProcessManager.h"
#include "ScreenCapture.h"
#include "OcrEngine.h"
#include "MemoryScanner.h"
#include "TrainerController.h"

class App {
public:
    App();
    ~App();

    bool Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
    void Render();
    void Cleanup();

private:
    HWND m_hwnd = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;

    // Core subsystems
    ProcessManager m_procMgr;
    ScreenCapture m_capture;
    AppOcrEngine m_ocr;
    MemoryScanner m_scanner;
    TrainerController m_trainer;

    // UI State
    std::vector<ProcessInfo> m_processList;
    int m_selectedProcessIdx = -1;
    char m_processFilter[128] = { 0 };

    CaptureRegion m_uiRegion;
    ID3D11ShaderResourceView* m_previewSRV = nullptr;
    int m_previewW = 0;
    int m_previewH = 0;

    int m_selectedDataTypeIdx = 0; // 0=Int32, 1=Int64, 2=Float, 3=Double, 4=String
    int m_targetThreshold = 3;
    bool m_autoContinuousMode = true;

    char m_customWriteBuffer[64] = "999999";
    uintptr_t m_selectedCandidateAddress = 0;
    bool m_requestOpenEditPopup = false;

    void ApplyTheme();
    void RenderTopBar();
    void RenderLeftPanel();
    void RenderCenterPanel();
    void RenderBottomPanel();
    void RefreshProcessList();
};
