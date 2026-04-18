#pragma once

#include <cstdint>
#include <vector>

namespace eyelib {
namespace rendering {

/** Draw bucket for ordering and pass selection (opaque first, sky last, etc.). */
enum class DrawLayer : uint8_t {
    Opaque = 0,
    AlphaTest,
    Transparent,
    Sky,
};

namespace draw_item_flags {
inline constexpr uint32_t kNone = 0;
inline constexpr uint32_t kDoubleSided = 1u << 0;
inline constexpr uint32_t kCastShadow = 1u << 1;
}  // namespace draw_item_flags

/**
 * One visible draw submission after culling. `sortKey` is filled by `VisibilitySystem` (A.4);
 * until then it stays 0. `RenderWorld` owns the backing `DrawList` for the frame; features read-only.
 */
struct DrawItem {
    uint32_t meshHandle = 0;
    uint32_t materialId = 0;
    uint32_t transformIndex = 0;
    uint64_t sortKey = 0;
    uint32_t flags = draw_item_flags::kNone;
    DrawLayer layer = DrawLayer::Opaque;
};

/** Typed CPU draw list produced by `VisibilitySystem` each frame. */
struct DrawList {
    std::vector<DrawItem> items;

    void Clear() noexcept { items.clear(); }

    /** Same as `items.size()`; use for scheduling / debug without duplicating state. */
    uint32_t VisibleCount() const noexcept { return static_cast<uint32_t>(items.size()); }
};

}  // namespace rendering
}  // namespace eyelib
