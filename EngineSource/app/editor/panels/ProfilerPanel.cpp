#include "ProfilerPanel.h"
#include "EditorContext.h"
#include "PanelCommon.h"

#include "api/debug/All.h"
#include "api/render/RenderPipeline.h"

#include <imgui.h>
#include <algorithm>
#include <vector>
#include <string>

namespace openeye::editor::panels {

namespace {

constexpr size_t kFrameHistoryCount = 120;

void UpdateFrameHistory(std::vector<float>& history, float frameMs) {
    if (history.size() >= kFrameHistoryCount)
        history.erase(history.begin());
    history.push_back(frameMs);
}

} // namespace

void ShowProfilerPanel(EditorContext* ctx) {
    if (!ImGui::Begin("Profiler", nullptr, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    const double frameMs = eyelib::api::debug::Profiler::GetLastFrameMs();
    const double fps = (frameMs > 0.0) ? (1000.0 / frameMs) : 0.0;

    ImGui::Text("FPS: %.1f", fps);
    ImGui::SameLine(0.f, 20.f);
    ImGui::Text("Frame: %.2f ms", frameMs);

    if (eyelib::api::debug::Presets::GetFrameTimeGraph()) {
        static std::vector<float> s_frameHistory;
        UpdateFrameHistory(s_frameHistory, static_cast<float>(frameMs));
        if (!s_frameHistory.empty()) {
            float maxMs = 0.f;
            for (float v : s_frameHistory)
                if (v > maxMs) maxMs = v;
            if (maxMs < 1.f) maxMs = 1.f;
            ImGui::PlotLines("Frame (ms)", s_frameHistory.data(), static_cast<int>(s_frameHistory.size()),
                0, nullptr, 0.f, maxMs * 1.1f, ImVec2(-1, 60));
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("CPU (last frame)");
    ImGui::BeginChild("CPU", ImVec2(-1, 120), true, ImGuiWindowFlags_None);
    {
        std::vector<eyelib::api::debug::ProfileSample> samples = eyelib::api::debug::Profiler::GetLastFrameSamples();
        if (samples.empty()) {
            ImGui::TextUnformatted("(no samples)");
        } else {
            if (ImGui::BeginTable("CPU", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Ms", ImGuiTableColumnFlags_WidthFixed, 60.f);
                ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_WidthFixed, 60.f);
                ImGui::TableHeadersRow();
                for (const auto& s : samples) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(s.name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.3f", s.milliseconds);
                    ImGui::TableSetColumnIndex(2);
                    double avg = eyelib::api::debug::Profiler::GetAverageTime(s.name);
                    ImGui::Text("%.3f", avg);
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextUnformatted("GPU (frame graph)");
    eyelib::api::RenderPipeline* pipeline = ctx ? ctx->sceneViewPipeline : nullptr;
    if (!pipeline) {
        ImGui::TextUnformatted("(no scene view pipeline)");
    } else {
        std::vector<eyelib::api::RenderPipeline::FrameGraphPassInfo> gpuPasses = pipeline->GetFrameGraphPasses();
        ImGui::BeginChild("GPU", ImVec2(-1, 140), true, ImGuiWindowFlags_None);
        if (gpuPasses.empty()) {
            ImGui::TextUnformatted("(no passes - render a frame with Scene view)");
        } else {
            if (ImGui::BeginTable("GPU", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Ms", ImGuiTableColumnFlags_WidthFixed, 60.f);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30.f);
                ImGui::TableHeadersRow();
                double totalGpu = 0.0;
                for (const auto& p : gpuPasses) {
                    if (p.culled) continue;
                    totalGpu += p.gpuMs;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(p.name.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.3f", p.gpuMs);
                    ImGui::TableSetColumnIndex(2);
                }
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Total (GPU)");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", totalGpu);
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
    }

    ImGui::Separator();
    if (ImGui::Button("Reset stats")) {
        eyelib::api::debug::Profiler::Reset();
    }
    ImGui::SameLine();
    bool showGraph = eyelib::api::debug::Presets::GetFrameTimeGraph();
    if (ImGui::Checkbox("Frame time graph", &showGraph)) {
        eyelib::api::debug::Presets::SetFrameTimeGraph(showGraph);
    }

    ImGui::End();
}

} // namespace openeye::editor::panels
