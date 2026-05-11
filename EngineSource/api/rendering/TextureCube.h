#pragma once

#include <cstdint>
#include <memory>

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Public PIMPL handle to a cubemap GPU texture (6 faces).
 *
 * Opaque to clients: the underlying internal type (`eyelib::rendering::TextureCube`) is
 * never named in this header. Construction deferred to **M3** — see `Mesh.h` docstring.
 *
 * Used by:
 *  - Skybox (M3.x — sky panel migration; currently a parity slot).
 *  - IBL probe sources (M3.x — IBL migration).
 */
class TextureCube {
public:
    TextureCube() noexcept;
    ~TextureCube();

    TextureCube(const TextureCube& other);
    TextureCube& operator=(const TextureCube& other);
    TextureCube(TextureCube&& other) noexcept;
    TextureCube& operator=(TextureCube&& other) noexcept;

    bool IsValid() const noexcept;

    /** Per-face edge length in pixels (0 when invalid). */
    int GetSize() const noexcept;

    /** Backend-native handle (Vulkan: `rhi::ITexture*`). Null when invalid. */
    void* GetNativeHandle() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    friend class TextureCubeAccess;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
