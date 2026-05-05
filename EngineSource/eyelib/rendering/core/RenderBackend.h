#pragma once

namespace eyelib {
namespace rendering {

/** Runtime render backend selection. Used by device creation and pipeline init. */
enum class RenderBackend {
    Default,
    Vulkan,
};

}  // namespace rendering
}  // namespace eyelib
