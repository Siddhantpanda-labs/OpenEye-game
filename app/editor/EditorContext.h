#pragma once

#include "api/world/All.h"

#include <string>

namespace openeye::editor {

/**
 * Shared editor state: world (in-memory scene), current scene file path, selection.
 * All mutations go through context.world (API only).
 */
struct EditorContext {
    eyelib::api::world::World world;
    std::string scenePath;  // empty if never loaded/saved

    /** Selected entity in hierarchy; InvalidEntity when none selected. */
    ::eyelib::world::EntityId selectedEntity = ::eyelib::world::InvalidEntity;

    enum class Modal { None, Open, SaveAs };
    Modal activeModal = Modal::None;
    char pathBuffer[512] = "scenes/main.json";
};

} // namespace openeye::editor
