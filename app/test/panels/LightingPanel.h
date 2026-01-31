#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace api {
namespace render {
struct DirectionalLight;
}
}
namespace test {

/** Floating ImGui panel for directional light (direction, color, intensity, ambient, exposure). */
class LightingPanel {
public:
    /** Draw the Lighting window. Edits light in place. */
    static void Render(api::render::DirectionalLight& light);
};
} // namespace test
} // namespace eyelib
