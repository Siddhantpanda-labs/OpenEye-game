// EyelibRenderingAPI — anchor TU.
//
// This translation unit exists so the static library has at least one .cpp to compile
// while façade types are added incrementally. Acts as a compile guard for public headers.

#include "api/rendering/CameraView.h"
#include "api/rendering/RenderBackend.h"
#include "api/rendering/RenderDevice.h"
#include "api/rendering/RenderTarget.h"

namespace eyelib {
namespace api {
namespace rendering {

namespace detail {

// Compile-time sanity: ensure opaque handle stays the size of a single pointer
// (mirrors the legacy api::RenderDevice ABI shape so editor/player swap is mechanical).
static_assert(sizeof(RenderDevice) == sizeof(void*),
              "eyelib::api::rendering::RenderDevice must remain a single-pointer opaque handle.");

} // namespace detail

} // namespace rendering
} // namespace api
} // namespace eyelib
