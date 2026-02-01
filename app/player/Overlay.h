#pragma once

#include "PlayerConfig.h"
#include "api/platform/Window.h"
#include "api/world/All.h"

namespace openeye::player {

class RuntimeOverlay {
public:
    bool Initialize(eyelib::api::Window& window);
    void Render(const RuntimeOptions& options, const eyelib::api::world::World& world);
    void Shutdown();
    bool IsReady() const { return m_ready; }

private:
    bool m_ready = false;
};

} // namespace openeye::player
