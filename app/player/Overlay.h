#pragma once

#include "PlayerConfig.h"
#include "api/platform/Window.h"
#include "api/world/All.h"

#include <string>
#include <cstddef>

namespace openeye::player {

/** Data for the overlay UI filled by the game thread; consumed only by the render thread. */
struct OverlaySnapshot {
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    size_t entityCount = 0;
    std::string scenePath;
};

class RuntimeOverlay {
public:
    bool Initialize(eyelib::api::Window& window);
    /** Render using snapshot only (render-thread safe; no World access). */
    void Render(const OverlaySnapshot& snapshot);
    void Shutdown();
    bool IsReady() const { return m_ready; }

private:
    bool m_ready = false;
};

} // namespace openeye::player
