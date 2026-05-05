#pragma once

#include <array>
#include <cstdint>

namespace eyelib {
namespace rendering {

/**
 * Backend-agnostic attachment load/store contract declared by frame-graph passes.
 * Vulkan/D3D/Metal backends map this to native render-pass/dynamic-rendering ops.
 */
enum class AttachmentLoadOp : uint8_t {
    Load = 0,
    Clear,
    DontCare,
};

enum class AttachmentStoreOp : uint8_t {
    Store = 0,
    DontCare,
};

struct ColorClearValue {
    std::array<float, 4> rgba{0.f, 0.f, 0.f, 1.f};
};

struct DepthStencilClearValue {
    float depth = 1.0f;
    uint32_t stencil = 0u;
};

struct ColorAttachmentOps {
    AttachmentLoadOp loadOp = AttachmentLoadOp::Load;
    AttachmentStoreOp storeOp = AttachmentStoreOp::Store;
    ColorClearValue clearValue{};
};

struct DepthAttachmentOps {
    AttachmentLoadOp loadOp = AttachmentLoadOp::Load;
    AttachmentStoreOp storeOp = AttachmentStoreOp::Store;
    DepthStencilClearValue clearValue{};
};

/**
 * Pass-declared attachment policy for render-target begin.
 * `hasDepth` gates whether `depth` should be consumed.
 */
struct RenderAttachmentOps {
    ColorAttachmentOps color{};
    DepthAttachmentOps depth{};
    bool hasDepth = true;
};

}  // namespace rendering
}  // namespace eyelib

