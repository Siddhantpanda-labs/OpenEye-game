#include "AIPlaygroundUI.h"

#include "api/core/All.h"
#include "eyelib/render/rhi/RHI.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#ifdef OPENEYE_IMGUI_VULKAN
#include <vulkan/vulkan.h>
#include <backends/imgui_impl_vulkan.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <string>

namespace openeye::ai_playground {

namespace {

void TrimCopyTo(const char* src, size_t maxLen, std::string& out) {
    out.assign(src ? src : "");
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.front()))) {
        out.erase(out.begin());
    }
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) {
        out.pop_back();
    }
    (void)maxLen;
}

constexpr size_t kMaxChatLines = 300;

void AppendUserAndDispatch(AIPlaygroundChatState& chat, const std::string& userLine, const ChatSubmitHandler& onSubmit) {
    chat.messages.push_back("user: " + userLine);
    if (onSubmit) {
        onSubmit(chat, userLine);
    } else {
        chat.messages.push_back("assistant: (internal) No submit handler.");
    }
    while (chat.messages.size() > kMaxChatLines) {
        chat.messages.erase(chat.messages.begin(), chat.messages.begin() + static_cast<std::ptrdiff_t>(chat.messages.size() - kMaxChatLines));
    }
}

} // namespace

bool AIPlaygroundUI::Initialize(eyelib::api::Window& window, eyelib::render::RenderBackend backend, void* device) {
    std::memset(m_newPlaygroundName, 0, sizeof(m_newPlaygroundName));
    m_newPlaygroundUseTemplate = false;
    m_openPlaygroundIndex = 0;
    m_vulkanDevice = nullptr;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

#ifdef OPENEYE_IMGUI_VULKAN
    if (backend == eyelib::render::RenderBackend::Vulkan && device) {
        auto* rhiDevice = static_cast<eyelib::render::rhi::IDevice*>(device);
        eyelib::render::rhi::ImGuiVulkanInitInfo info = {};
        if (!rhiDevice->GetImGuiVulkanInitInfo(&info)) {
            ImGui::DestroyContext();
            return false;
        }
        if (!ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(window.GetNativeHandle()), true)) {
            ImGui::DestroyContext();
            return false;
        }
        ImGui_ImplVulkan_InitInfo vkInfo = {};
        vkInfo.ApiVersion = VK_API_VERSION_1_0;
        vkInfo.Instance = static_cast<VkInstance>(info.instance);
        vkInfo.PhysicalDevice = static_cast<VkPhysicalDevice>(info.physical_device);
        vkInfo.Device = static_cast<VkDevice>(info.device);
        vkInfo.QueueFamily = info.queue_family;
        vkInfo.Queue = static_cast<VkQueue>(info.queue);
        vkInfo.DescriptorPool = static_cast<VkDescriptorPool>(info.descriptor_pool);
        vkInfo.RenderPass = static_cast<VkRenderPass>(info.render_pass);
        vkInfo.MinImageCount = info.min_image_count;
        vkInfo.ImageCount = info.image_count;
        vkInfo.PipelineCache = static_cast<VkPipelineCache>(info.pipeline_cache);
        if (!ImGui_ImplVulkan_Init(&vkInfo)) {
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            return false;
        }
        m_vulkanDevice = device;
        rhiDevice->SetPostBlitCallback([](void* cmd) {
            // Defensive guard: callback can survive teardown/reload boundaries for a frame; avoid touching ImGui without a context.
            if (ImGui::GetCurrentContext() == nullptr) {
                return;
            }
            ImDrawData* drawData = ImGui::GetDrawData();
            if (!drawData) {
                return;
            }
            ImGui_ImplVulkan_RenderDrawData(drawData, static_cast<VkCommandBuffer>(cmd));
        });
        m_vulkan = true;
        m_ready = true;
        return true;
    }
#endif
    if (backend == eyelib::render::RenderBackend::Vulkan) {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(window.GetNativeHandle()), true)) {
        ImGui::DestroyContext();
        return false;
    }
    ImGui_ImplOpenGL3_Init("#version 450");
    m_ready = true;
    return true;
}

void AIPlaygroundUI::Render(const AIPlaygroundSnapshot& snapshot, AIPlaygroundChatState& chat, ChatSubmitHandler onSubmit,
                            const PlaygroundUIMenu* playgroundMenu) {
    if (!m_ready) {
        return;
    }

    if (m_vulkan) {
        ImGui_ImplVulkan_NewFrame();
    } else {
        ImGui_ImplOpenGL3_NewFrame();
    }
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const ImGuiWindowFlags cornerFlags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoNav;

    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("AI Playground##stats", nullptr, cornerFlags)) {
        ImGui::TextUnformatted("OpenEye AI Playground");
        ImGui::Separator();
        if (playgroundMenu) {
            if (snapshot.workspaceFound) {
                if (ImGui::Button("New Playground")) {
                    ImGui::OpenPopup("NewPlayground");
                }
                ImGui::SameLine();
                if (ImGui::Button("Open Playground")) {
                    if (!playgroundMenu->existingNames.empty()) {
                        m_openPlaygroundIndex = std::min(m_openPlaygroundIndex,
                            static_cast<int>(playgroundMenu->existingNames.size()) - 1);
                    }
                    ImGui::OpenPopup("OpenPlayground");
                }
            } else {
                ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), "Workspace not found");
                ImGui::TextUnformatted("(run from OpenEye repo / install layout)");
            }
            ImGui::Text("Project: %s", snapshot.projectName.empty() ? "(none)" : snapshot.projectName.c_str());
            if (!snapshot.projectPathRelative.empty()) {
                ImGui::TextDisabled("%s", snapshot.projectPathRelative.c_str());
            }
            if (playgroundMenu->lastError && !playgroundMenu->lastError->empty()) {
                ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", playgroundMenu->lastError->c_str());
            }
            ImGui::Separator();
        }
        ImGui::Text("Scene: %s", snapshot.scenePath.c_str());
        if (playgroundMenu && playgroundMenu->onSaveScene) {
            const bool canSave = snapshot.simulationCanStart;
            if (!canSave) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Save scene")) {
                playgroundMenu->onSaveScene();
            }
            if (!canSave) {
                ImGui::EndDisabled();
            }
            if (!canSave) {
                ImGui::SameLine();
                ImGui::TextDisabled("(open a project or --scene)");
            }
        }
        ImGui::Text("FPS: %.1f", snapshot.fps);
        ImGui::Text("Frame: %.3f ms", snapshot.frameTimeMs);
        ImGui::Text("Entities: %zu", snapshot.entityCount);
        ImGui::Text("LLM: %s", snapshot.llmStatus.c_str());
        ImGui::Separator();
        if (playgroundMenu && (playgroundMenu->onSimulationPlay || playgroundMenu->onSimulationStop)) {
            const bool running = snapshot.simulationRunning;
            const bool canStart = snapshot.simulationCanStart;
            ImGui::TextUnformatted("Simulation");
            ImGui::SameLine();
            if (ImGui::Button("Play")) {
                if (playgroundMenu->onSimulationPlay && canStart && !running) {
                    playgroundMenu->onSimulationPlay();
                }
            }
            if (!canStart && !running) {
                ImGui::SameLine();
                ImGui::TextDisabled("(load a scene)");
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop")) {
                if (playgroundMenu->onSimulationStop && running) {
                    playgroundMenu->onSimulationStop();
                }
            }
            ImGui::Separator();
        }
        ImGui::TextDisabled("3D (edit): WASD+QE, RMB look — keys yield to text fields; hold RMB to move while UI has keyboard focus");

        if (playgroundMenu) {
            if (ImGui::BeginPopupModal("NewPlayground", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Create under Projects/AI_Playground/<name>");
                ImGui::InputText("Name", m_newPlaygroundName, sizeof(m_newPlaygroundName));
                ImGui::Checkbox("Load packaged template (assets/AI_Playground/default.scene)", &m_newPlaygroundUseTemplate);
                if (ImGui::Button("Create", ImVec2(120, 0)) && playgroundMenu->onNew) {
                    std::string trimmed;
                    TrimCopyTo(m_newPlaygroundName, sizeof(m_newPlaygroundName), trimmed);
                    playgroundMenu->onNew(trimmed, m_newPlaygroundUseTemplate);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopupModal("OpenPlayground", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                if (playgroundMenu->existingNames.empty()) {
                    ImGui::TextUnformatted("No playgrounds yet. Use New Playground.");
                } else {
                    ImGui::TextUnformatted("Select a playground:");
                    const char* preview = playgroundMenu->existingNames[static_cast<size_t>(m_openPlaygroundIndex)].c_str();
                    if (ImGui::BeginCombo("##pgopen", preview)) {
                        for (int i = 0; i < static_cast<int>(playgroundMenu->existingNames.size()); ++i) {
                            const bool sel = (i == m_openPlaygroundIndex);
                            if (ImGui::Selectable(playgroundMenu->existingNames[static_cast<size_t>(i)].c_str(), sel)) {
                                m_openPlaygroundIndex = i;
                            }
                            if (sel) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                if (ImGui::Button("Open", ImVec2(120, 0)) && playgroundMenu->onOpen
                    && !playgroundMenu->existingNames.empty()) {
                    const std::string& pick = playgroundMenu->existingNames[static_cast<size_t>(m_openPlaygroundIndex)];
                    playgroundMenu->onOpen(pick);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
    }
    ImGui::End();

    const float chatWidth = 360.0f;
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - chatWidth - 8.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(chatWidth, ImGui::GetIO().DisplaySize.y - 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    if (ImGui::Begin("World commands##chat", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        ImGui::TextUnformatted("Chat → World (/observe /plan /ai demo; /ask uses provider below)");
        ImGui::Combo("LLM for /ask", &chat.llmProviderIndex, "ChatGPT (OpenAI)\0Gemini (Google)\0");
        ImGui::Separator();
        const float footer = 110.0f;
        const float historyH = std::max(120.0f, ImGui::GetContentRegionAvail().y - footer);
        if (ImGui::BeginChild("##history", ImVec2(0, historyH), true)) {
            for (const std::string& line : chat.messages) {
                ImGui::TextWrapped("%s", line.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::InputTextMultiline("##input", chat.inputBuffer, sizeof(chat.inputBuffer), ImVec2(-1, 72));
        if (ImGui::Button("Send", ImVec2(80, 0))) {
            std::string line;
            TrimCopyTo(chat.inputBuffer, sizeof(chat.inputBuffer), line);
            if (!line.empty()) {
                chat.inputBuffer[0] = '\0';
                AppendUserAndDispatch(chat, line, onSubmit);
            }
        }
    }
    ImGui::End();

    ImGui::Render();
    if (!m_vulkan) {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

void AIPlaygroundUI::Shutdown() {
    if (!m_ready) {
        return;
    }
    if (m_vulkan) {
#ifdef OPENEYE_IMGUI_VULKAN
        if (m_vulkanDevice) {
            static_cast<eyelib::render::rhi::IDevice*>(m_vulkanDevice)->SetPostBlitCallback({});
        }
        ImGui_ImplVulkan_Shutdown();
#endif
    } else {
        ImGui_ImplOpenGL3_Shutdown();
    }
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_ready = false;
    m_vulkan = false;
    m_vulkanDevice = nullptr;
}

} // namespace openeye::ai_playground
