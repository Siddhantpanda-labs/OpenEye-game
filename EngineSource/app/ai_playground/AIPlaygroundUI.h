#pragma once

#include "api/platform/Window.h"
#include "api/render/RenderBackend.h"

#include <functional>
#include <string>
#include <vector>

namespace openeye::ai_playground {

struct AIPlaygroundSnapshot {
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    size_t entityCount = 0;
    std::string scenePath;
    /** Shown in chat panel header (e.g. Disconnected | Ready). */
    std::string llmStatus = "Disconnected";
    /** Workspace (repo root) detected; enables New/Open. */
    bool workspaceFound = false;
    /** Playground name or "(none)". */
    std::string projectName;
    /** e.g. Projects/AI_Playground/MyName */
    std::string projectPathRelative;
    /** True while physics, scripts, and world systems are advancing (Play mode). */
    bool simulationRunning = false;
    /** Play is allowed (scene loaded, runtime ready). */
    bool simulationCanStart = false;
};

struct AIPlaygroundChatState {
    std::vector<std::string> messages;
    char inputBuffer[2048]{};
};

/** Called after the user sends a line; append assistant/system lines to \a chat.messages. */
using ChatSubmitHandler = std::function<void(AIPlaygroundChatState& chat, const std::string& userLine)>;

/** New/Open playground: \a name is raw from UI (trimmed); host sanitizes and may reject. */
using PlaygroundNewHandler = std::function<void(const std::string& name, bool loadPackagedTemplate)>;
using PlaygroundOpenHandler = std::function<void(const std::string& name)>;

struct PlaygroundUIMenu {
    std::vector<std::string> existingNames;
    PlaygroundNewHandler onNew;
    PlaygroundOpenHandler onOpen;
    /** Optional: show last create/open error until cleared. */
    std::string* lastError = nullptr;
    /** Host-owned; begin/end simulation (editor-style Play / Stop). */
    std::function<void()> onSimulationPlay;
    std::function<void()> onSimulationStop;
};

/**
 * ImGui overlay: stats corner + dock-style chat panel.
 * Vulkan: records in device post-blit callback (same pattern as player RuntimeOverlay).
 */
class AIPlaygroundUI {
public:
    bool Initialize(eyelib::api::Window& window, eyelib::render::RenderBackend backend, void* device);
    void Render(const AIPlaygroundSnapshot& snapshot, AIPlaygroundChatState& chat, ChatSubmitHandler onSubmit,
                const PlaygroundUIMenu* playgroundMenu);
    void Shutdown();

    bool IsReady() const { return m_ready; }
    bool IsVulkan() const { return m_vulkan; }

private:
    bool m_ready = false;
    bool m_vulkan = false;
    /** Vulkan-only: owning runtime RHI device for post-blit callback clear on shutdown. */
    void* m_vulkanDevice = nullptr;
    char m_newPlaygroundName[128]{};
    bool m_newPlaygroundUseTemplate = false;
    int m_openPlaygroundIndex = 0;
};

} // namespace openeye::ai_playground
