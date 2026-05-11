#pragma once

#include "eyelib/math/Types.h"

namespace eyelib {
namespace rendering {

/**
 * Plain colored line segment consumed by `EditorFeature` + `DebugDrawPass`.
 *
 * Frame-local: callers (api/rendering adapter) refill the line list every frame from the
 * editor / debug source and hand it to `EditorFeature::SetLines`. The feature copies the
 * span into its own buffer so the caller's storage may be destroyed before pass execution.
 *
 * Coordinate space is world-space; the vertex shader applies the active `PerView::uViewProjection`.
 * Colour is linear RGBA (alpha currently treated as opaque by the v1 pipeline; reserved for a
 * future blend-state override on the line pipeline).
 */
struct DebugLine {
    math::Vec3 a{0.0f, 0.0f, 0.0f};
    math::Vec3 b{0.0f, 0.0f, 0.0f};
    math::Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

}  // namespace rendering
}  // namespace eyelib
