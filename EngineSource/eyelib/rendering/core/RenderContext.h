#pragma once

#include <cstdint>

namespace eyelib {
namespace rendering {

class IRenderSceneView;
class RenderResources;
struct DrawList;

namespace rhi {
struct IDevice;
struct ICommandList;
}

/** Non-owning per-frame inputs: device, output size, optional scene view, optional CPU draw list. */
struct RenderContext {
    rhi::IDevice* device = nullptr;
    uint32_t outputWidth = 0;
    uint32_t outputHeight = 0;
    const IRenderSceneView* sceneView = nullptr;
    /** Filled by `RenderWorld` before `OnUpload` / scheduling when visibility runs. */
    const DrawList* drawList = nullptr;
    /** Active list for this frame (`RunFrame` argument); null if not recording. */
    rhi::ICommandList* recordingCommandList = nullptr;
    /** `RenderWorld` system resources; valid for the duration of `RunFrame` after upload prep. Use for upload helpers / buffer accessors. */
    RenderResources* resources = nullptr;
};

}  // namespace rendering
}  // namespace eyelib
