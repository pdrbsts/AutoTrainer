#include "App.h"
#include "Utils.h"
#include "RegionSelectorOverlay.h"
#include "imgui.h"
#include <algorithm>

App::App()
    : m_trainer(m_procMgr, m_capture, m_ocr, m_scanner) {
    m_uiRegion = m_capture.GetRegion();
}

App::~App() {
    Cleanup();
}

bool App::Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context) {
    m_hwnd = hwnd;
    m_device = device;
    m_context = context;

    ApplyTheme();

    LOG_INFO("AutoTrainer started.");
    m_ocr.Initialize();
    RefreshProcessList();

    return true;
}

void App::Cleanup() {
    m_capture.ReleaseTexture();
}

void App::RefreshProcessList() {
    m_processList = m_procMgr.EnumerateGameProcesses();
}

void App::ApplyTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(8, 6);
    style.ItemSpacing = ImVec2(8, 8);
    style.ItemInnerSpacing = ImVec2(6, 6);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 12.0f;

    // Dark sleek theme with emerald & cyan accents
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.94f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.55f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.15f, 0.18f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.22f, 0.25f, 0.29f, 0.80f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.28f, 0.33f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.32f, 0.38f, 1.00f);

    colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);

    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.28f, 0.33f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.32f, 0.36f, 0.42f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.38f, 0.43f, 0.50f, 1.00f);

    colors[ImGuiCol_CheckMark] = ImVec4(0.15f, 0.85f, 0.55f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.15f, 0.75f, 0.55f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.20f, 0.88f, 0.65f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.26f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.34f, 0.46f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.42f, 0.58f, 1.00f);

    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.26f, 0.33f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.34f, 0.43f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.40f, 0.50f, 1.00f);

    colors[ImGuiCol_Separator] = ImVec4(0.22f, 0.25f, 0.29f, 0.80f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.28f, 0.34f, 0.42f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.32f, 0.42f, 0.55f, 1.00f);

    colors[ImGuiCol_ResizeGrip] = ImVec4(0.20f, 0.24f, 0.28f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.30f, 0.36f, 0.44f, 0.80f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.15f, 0.85f, 0.55f, 1.00f);

    colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.16f, 0.19f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.28f, 0.36f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.24f, 0.32f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.19f, 0.23f, 1.00f);

    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.25f, 0.28f, 0.33f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.14f, 0.15f, 0.18f, 0.50f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.16f, 0.17f, 0.20f, 0.50f);
}

void App::Render() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));

    ImGui::Begin("AutoTrainerMainWindow", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    // Update screen capture preview texture
    if (m_device) {
        m_capture.UpdateDx11Texture(m_device, &m_previewSRV, m_previewW, m_previewH);
    }

    RenderTopBar();
    ImGui::Separator();
    ImGui::Spacing();

    // Main layout: 2 Columns (Left: OCR & Capture, Right: Memory Scanner & Table)
    float leftWidth = 380.0f;
    float rightWidth = ImGui::GetContentRegionAvail().x - leftWidth - 12.0f;
    float contentHeight = ImGui::GetContentRegionAvail().y - 140.0f;

    ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, contentHeight), true);
    RenderLeftPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("CenterPanel", ImVec2(rightWidth, contentHeight), true);
    RenderCenterPanel();
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("BottomPanel", ImVec2(0, 0), true);
    RenderBottomPanel();
    ImGui::EndChild();

    ImGui::End();
}

void App::RenderTopBar() {
    ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.6f, 1.0f), "AUTOTRAINER");
    ImGui::SameLine();
    ImGui::TextDisabled("v1.0 - Screen OCR & Memory Scanner");

    float comboWidth = 220.0f;
    float refreshBtnWidth = 70.0f;
    float attachBtnWidth = 75.0f;
    float totalControlsWidth = comboWidth + refreshBtnWidth + attachBtnWidth + (ImGui::GetStyle().ItemSpacing.x * 2.0f);

    float targetPos = ImGui::GetWindowWidth() - totalControlsWidth - ImGui::GetStyle().WindowPadding.x - 15.0f;
    if (targetPos > 320.0f) {
        ImGui::SameLine(targetPos);
    } else {
        ImGui::SameLine();
    }

    // Process selection & Attach
    std::string previewText = "Select Target Process...";
    if (m_procMgr.IsAttached()) {
        previewText = StringUtils::WideToUtf8(m_procMgr.GetCurrentProcessInfo().processName) +
                      " (PID: " + std::to_string(m_procMgr.GetPID()) + ")";
    } else if (m_selectedProcessIdx >= 0 && m_selectedProcessIdx < (int)m_processList.size()) {
        previewText = StringUtils::WideToUtf8(m_processList[m_selectedProcessIdx].processName) +
                      " (PID: " + std::to_string(m_processList[m_selectedProcessIdx].pid) + ")";
    }

    ImGui::SetNextItemWidth(comboWidth);
    if (ImGui::BeginCombo("##ProcessCombo", previewText.c_str())) {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##ProcFilter", "Filter...", m_processFilter, sizeof(m_processFilter));
        ImGui::Separator();

        for (int i = 0; i < (int)m_processList.size(); ++i) {
            const auto& proc = m_processList[i];
            std::string procName = StringUtils::WideToUtf8(proc.processName);
            std::string title = StringUtils::WideToUtf8(proc.windowTitle);

            if (m_processFilter[0] != '\0') {
                std::string filter = m_processFilter;
                std::string lowerProc = procName;
                std::string lowerTitle = title;
                std::transform(lowerProc.begin(), lowerProc.end(), lowerProc.begin(), ::tolower);
                std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::tolower);
                std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

                if (lowerProc.find(filter) == std::string::npos && lowerTitle.find(filter) == std::string::npos) {
                    continue;
                }
            }

            std::string label = procName + " [" + std::to_string(proc.pid) + "] - " + title;
            bool isSelected = (m_selectedProcessIdx == i);

            if (ImGui::Selectable(label.c_str(), isSelected)) {
                m_selectedProcessIdx = i;
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Refresh", ImVec2(refreshBtnWidth, 0))) {
        RefreshProcessList();
    }

    ImGui::SameLine();
    if (!m_procMgr.IsAttached()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.32f, 1.0f));
        if (ImGui::Button("Attach", ImVec2(attachBtnWidth, 0))) {
            if (m_selectedProcessIdx >= 0 && m_selectedProcessIdx < (int)m_processList.size()) {
                DWORD pid = m_processList[m_selectedProcessIdx].pid;
                if (m_procMgr.OpenTarget(pid)) {
                    m_scanner.SetProcessHandle(m_procMgr.GetHandle(), m_procMgr.GetCurrentProcessInfo().is64Bit);
                }
            } else {
                LOG_WARNING("Please select a target process from the dropdown list first.");
            }
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.22f, 0.22f, 1.0f));
        if (ImGui::Button("Detach", ImVec2(attachBtnWidth, 0))) {
            m_trainer.Reset();
            m_procMgr.CloseTarget();
            m_scanner.SetProcessHandle(nullptr, true);
            LOG_INFO("Detached from process.");
        }
        ImGui::PopStyleColor();
    }
}

void App::RenderLeftPanel() {
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "SCREEN CAPTURE & OCR");
    ImGui::Separator();
    ImGui::Spacing();

    // Interactive Region Selection Button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.75f, 1.0f));
    if (ImGui::Button("🎯 Select Region On Screen", ImVec2(-1, 32))) {
        RegionSelectorOverlay::Get().StartSelection([this](const CaptureRegion& reg) {
            m_uiRegion = reg;
            m_capture.SetRegion(reg);
        });
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Coordinates configuration
    ImGui::Text("Region Box (Pixels):");
    ImGui::SetNextItemWidth(80);
    bool changed = ImGui::DragInt("X##RegX", &m_uiRegion.x, 1, 0, 7680);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::DragInt("Y##RegY", &m_uiRegion.y, 1, 0, 4320);

    ImGui::SetNextItemWidth(80);
    changed |= ImGui::DragInt("W##RegW", &m_uiRegion.width, 1, 10, 1920);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    changed |= ImGui::DragInt("H##RegH", &m_uiRegion.height, 1, 10, 1080);

    if (ImGui::Checkbox("Relative to Game Window", &m_uiRegion.relativeToWindow)) {
        changed = true;
    }

    if (changed) {
        m_capture.SetRegion(m_uiRegion);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Real-time Preview Texture
    ImGui::Text("Live Region Feed:");
    if (m_previewSRV && m_previewW > 0 && m_previewH > 0) {
        float previewBoxW = ImGui::GetContentRegionAvail().x;
        float aspect = (float)m_previewH / (float)m_previewW;
        float previewBoxH = std::min(140.0f, previewBoxW * aspect);
        if (previewBoxH < 40.0f) previewBoxH = 40.0f;

        ImGui::Image((ImTextureID)m_previewSRV, ImVec2(previewBoxW, previewBoxH), ImVec2(0, 0), ImVec2(1, 1),
                     ImVec4(1, 1, 1, 1), ImVec4(0.3f, 0.35f, 0.4f, 1.0f));
    } else {
        ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 60));
        ImGui::TextDisabled("No capture feed (Click 'Select Region')");
    }

    ImGui::Spacing();

    // Image Preprocessing Options
    if (ImGui::TreeNode("Image Pre-Processing")) {
        ImGui::Checkbox("Grayscale", &m_capture.applyGrayscale);
        ImGui::Checkbox("Binary Threshold", &m_capture.applyThreshold);
        if (m_capture.applyThreshold) {
            ImGui::SliderInt("Threshold", &m_capture.thresholdValue, 0, 255);
        }
        ImGui::Checkbox("Invert Colors", &m_capture.invertColors);
        ImGui::TreePop();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Live OCR Recognition Output
    ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.2f, 1.0f), "LIVE OCR DETECTION");

    TrainerOcrResult ocr = m_trainer.GetLastOcrResult();

    ImGui::BeginChild("OcrBox", ImVec2(0, 95), true);
    if (ocr.isValidNumber) {
        ImGui::Text("Recognized Value:");
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.5f, 1.0f), "%s", StringUtils::FormatNumberWithCommas(ocr.parsedInt64).c_str());
        ImGui::PopFont();
        ImGui::TextDisabled("Raw Text: \"%s\" (Latency: %.1f ms)", ocr.fullText.c_str(), ocr.recognitionTimeMs);
    } else {
        ImGui::TextDisabled("Searching for numbers in region...");
        if (!ocr.fullText.empty()) {
            ImGui::TextDisabled("Detected text: \"%s\"", ocr.fullText.c_str());
        }
    }
    ImGui::EndChild();
}

void App::RenderCenterPanel() {
    TrainerState state = m_trainer.GetState();
    size_t candCount = m_scanner.GetCandidateCount();

    // Top State Banner
    ImVec4 bannerColor = ImVec4(0.2f, 0.25f, 0.3f, 1.0f);
    if (state == TrainerState::FirstScanning || state == TrainerState::FilterScanning) {
        bannerColor = ImVec4(0.6f, 0.45f, 0.1f, 1.0f);
    } else if (state == TrainerState::WaitingForValueChange) {
        bannerColor = ImVec4(0.2f, 0.45f, 0.7f, 1.0f);
    } else if (state == TrainerState::TargetLocked) {
        bannerColor = ImVec4(0.12f, 0.55f, 0.32f, 1.0f);
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, bannerColor);
    ImGui::BeginChild("StatusBanner", ImVec2(0, 48), true);
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "STATUS: %s", m_trainer.GetStateString().c_str());
    if (candCount > 0) {
        ImGui::SameLine(ImGui::GetWindowWidth() - 220);
        ImGui::Text("Candidates: %s", StringUtils::FormatNumberWithCommas(candCount).c_str());
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::Spacing();

    // Controls Row
    const char* dataTypes[] = { "Int32 (4 Bytes)", "Int64 (8 Bytes)", "Float (4 Bytes)", "Double (8 Bytes)" };
    ImGui::SetNextItemWidth(160);
    if (ImGui::Combo("Data Type", &m_selectedDataTypeIdx, dataTypes, IM_ARRAYSIZE(dataTypes))) {
        ScanDataType dt = ScanDataType::Int32;
        if (m_selectedDataTypeIdx == 1) dt = ScanDataType::Int64;
        else if (m_selectedDataTypeIdx == 2) dt = ScanDataType::Float;
        else if (m_selectedDataTypeIdx == 3) dt = ScanDataType::Double;
        m_scanner.SetDataType(dt);
    }

    ImGui::SameLine(320);
    ImGui::SetNextItemWidth(100);
    if (ImGui::SliderInt("Target Threshold", &m_targetThreshold, 1, 10)) {
        m_trainer.targetCandidateThreshold = m_targetThreshold;
    }

    ImGui::Spacing();

    // Auto-Trainer Action Buttons
    if (!m_trainer.IsAutoTrainerActive()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.65f, 0.35f, 1.0f));
        if (ImGui::Button("▶ Start AutoTrainer (Continuous Auto-Scan)", ImVec2(300, 36))) {
            m_trainer.StartAutoTrainer();
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.45f, 0.15f, 1.0f));
        if (ImGui::Button("⏸ Pause AutoTrainer", ImVec2(300, 36))) {
            m_trainer.StopAutoTrainer();
        }
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    if (ImGui::Button("1st Scan", ImVec2(90, 36))) {
        m_trainer.TriggerManualFirstScan();
    }

    ImGui::SameLine();
    if (ImGui::Button("Next Scan", ImVec2(90, 36))) {
        m_trainer.TriggerManualNextScan();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(80, 36))) {
        m_trainer.Reset();
    }

    // Progress bar during scan
    if (m_scanner.IsScanning()) {
        ImGui::Spacing();
        ImGui::ProgressBar(m_scanner.GetProgress(), ImVec2(-1, 16), "Scanning Memory...");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Candidate List / Memory Table
    ImGui::Text("Found Candidate Addresses (%zu total):", candCount);

    auto candidates = m_scanner.GetCandidates();

    if (ImGui::BeginTable("CandidatesTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 0))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Current Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Previous Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableHeadersRow();

        size_t displayLimit = std::min(candidates.size(), (size_t)200);
        for (size_t i = 0; i < displayLimit; ++i) {
            const auto& cand = candidates[i];
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%zu", i + 1);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", StringUtils::FormatAddress(cand.address).c_str());

            ImGui::TableSetColumnIndex(2);
            if (m_selectedDataTypeIdx == 0 || m_selectedDataTypeIdx == 1) {
                ImGui::Text("%s", StringUtils::FormatNumberWithCommas(cand.currentValueInt).c_str());
            } else {
                ImGui::Text("%.4f", cand.currentValueDouble);
            }

            ImGui::TableSetColumnIndex(3);
            if (m_selectedDataTypeIdx == 0 || m_selectedDataTypeIdx == 1) {
                ImGui::TextDisabled("%s", StringUtils::FormatNumberWithCommas(cand.previousValueInt).c_str());
            } else {
                ImGui::TextDisabled("%.4f", cand.previousValueDouble);
            }

            ImGui::TableSetColumnIndex(4);
            std::string lockId = "Freeze##" + std::to_string(cand.address);
            bool locked = cand.isLocked;
            if (ImGui::Checkbox(lockId.c_str(), &locked)) {
                ScanValue val;
                val.type = m_scanner.GetCurrentDataType();
                val.intVal = cand.currentValueInt;
                val.doubleVal = cand.currentValueDouble;
                m_scanner.SetAddressLock(cand.address, locked, val);
            }

            ImGui::SameLine();
            std::string editId = "Edit##" + std::to_string(cand.address);
            if (ImGui::SmallButton(editId.c_str())) {
                m_selectedCandidateAddress = cand.address;
                if (m_selectedDataTypeIdx == 0 || m_selectedDataTypeIdx == 1) {
                    snprintf(m_customWriteBuffer, sizeof(m_customWriteBuffer), "%lld", (long long)cand.currentValueInt);
                } else {
                    snprintf(m_customWriteBuffer, sizeof(m_customWriteBuffer), "%.4f", cand.currentValueDouble);
                }
                m_requestOpenEditPopup = true;
            }
        }

        ImGui::EndTable();
    }

    // Trigger popup outside of table scope
    if (m_requestOpenEditPopup) {
        ImGui::OpenPopup("Edit Value");
        m_requestOpenEditPopup = false;
    }

    // Modal popup to write custom value to selected address
    if (ImGui::BeginPopupModal("Edit Value", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Target Address: %s", StringUtils::FormatAddress(m_selectedCandidateAddress).c_str());
        ImGui::TextDisabled("Type: %s", dataTypes[m_selectedDataTypeIdx]);
        ImGui::Spacing();

        ImGui::Text("Enter New Value:");
        ImGui::SetNextItemWidth(260);
        bool enterPressed = ImGui::InputText("##NewValInput", m_customWriteBuffer, sizeof(m_customWriteBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::Spacing();
        ImGui::TextDisabled("Quick Presets:");
        if (ImGui::Button("+500")) {
            int64_t val = 0;
            StringUtils::ParseInt64(m_customWriteBuffer, val);
            snprintf(m_customWriteBuffer, sizeof(m_customWriteBuffer), "%lld", (long long)(val + 500));
        }
        ImGui::SameLine();
        if (ImGui::Button("+5,000")) {
            int64_t val = 0;
            StringUtils::ParseInt64(m_customWriteBuffer, val);
            snprintf(m_customWriteBuffer, sizeof(m_customWriteBuffer), "%lld", (long long)(val + 5000));
        }
        ImGui::SameLine();
        if (ImGui::Button("999,999")) {
            snprintf(m_customWriteBuffer, sizeof(m_customWriteBuffer), "999999");
        }
        ImGui::SameLine();
        if (ImGui::Button("Max 32-bit")) {
            snprintf(m_customWriteBuffer, sizeof(m_customWriteBuffer), "2147483647");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto doWrite = [this]() {
            ScanValue val;
            val.type = m_scanner.GetCurrentDataType();
            if (val.type == ScanDataType::Int32 || val.type == ScanDataType::Int64) {
                StringUtils::ParseInt64(m_customWriteBuffer, val.intVal);
                val.doubleVal = static_cast<double>(val.intVal);
            } else {
                StringUtils::ParseDouble(m_customWriteBuffer, val.doubleVal);
                val.intVal = static_cast<int64_t>(val.doubleVal);
            }
            m_scanner.WriteValue(m_selectedCandidateAddress, val);
        };

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.32f, 1.0f));
        if (ImGui::Button("Apply & Write", ImVec2(130, 28)) || enterPressed) {
            doWrite();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(90, 28))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void App::RenderBottomPanel() {
    ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.8f, 1.0f), "EVENT LOG & DIAGNOSTICS");
    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    if (ImGui::SmallButton("Clear Logs")) {
        Logger::Get().Clear();
    }

    ImGui::Separator();

    auto logs = Logger::Get().GetEntries();
    if (ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        for (const auto& entry : logs) {
            ImVec4 col = ImVec4(0.85f, 0.88f, 0.92f, 1.0f);
            if (entry.level == LogLevel::Success) col = ImVec4(0.2f, 0.9f, 0.5f, 1.0f);
            else if (entry.level == LogLevel::Warning) col = ImVec4(0.95f, 0.75f, 0.2f, 1.0f);
            else if (entry.level == LogLevel::Error) col = ImVec4(0.95f, 0.3f, 0.3f, 1.0f);

            ImGui::TextDisabled("[%s]", entry.timestamp.c_str());
            ImGui::SameLine();
            ImGui::TextColored(col, "%s", entry.message.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
}
