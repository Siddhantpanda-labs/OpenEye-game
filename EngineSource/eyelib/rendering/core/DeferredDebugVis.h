#pragma once

#include <array>

/**
 * Temporary deferred-path visualization (clear colors + logs).
 * Set to 0 when debugging is complete; changes are tracked in
 * documentation/Render_Module/render_fix_track.md
 */
#ifndef OPENEYE_RENDER_DEFERRED_DEBUG_PASS_COLORS
#define OPENEYE_RENDER_DEFERRED_DEBUG_PASS_COLORS 1
#endif

namespace eyelib {
namespace rendering {
namespace deferred_debug {

#if OPENEYE_RENDER_DEFERRED_DEBUG_PASS_COLORS

/** G-buffer MRT clear (all color attachments use the same clear in current pass contract). */
inline constexpr std::array<float, 4> kGBufferClearRgba = {0.75f, 0.0f, 0.85f, 1.0f};  // purple — step A */

/** Main HDR clear before fullscreen deferred lighting draw. */
inline constexpr std::array<float, 4> kHdrDeferredClearRgba = {0.95f, 0.55f, 0.05f, 1.0f};  // orange — step B */

#else

inline constexpr std::array<float, 4> kGBufferClearRgba = {0.0f, 0.0f, 0.0f, 1.0f};
inline constexpr std::array<float, 4> kHdrDeferredClearRgba = {0.1f, 0.15f, 0.2f, 1.0f};

#endif

}  // namespace deferred_debug
}  // namespace rendering
}  // namespace eyelib
