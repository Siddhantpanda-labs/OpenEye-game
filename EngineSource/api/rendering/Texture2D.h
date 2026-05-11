#pragma once

#include <cstdint>
#include <memory>

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Public PIMPL handle to a 2D GPU texture.
 *
 * Opaque to clients: the underlying internal type (`eyelib::rendering::Texture2D`) is
 * never named in this header. Construction is deferred to **M3** — see `Mesh.h` docstring
 * for design rationale.
 *
 * `GetNativeHandle()` returns a backend-specific opaque pointer:
 *   - Vulkan: pointer to the internal `rhi::ITexture` (further cast to `VkImageView` via
 *     `ITexture::GetImageView()` — used by ImGui's `AddTexture` API).
 * Returns null when invalid.
 */
class Texture2D {
public:
    Texture2D() noexcept;
    ~Texture2D();

    Texture2D(const Texture2D& other);
    Texture2D& operator=(const Texture2D& other);
    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;

    bool IsValid() const noexcept;

    int GetWidth() const noexcept;
    int GetHeight() const noexcept;

    /** Backend-native handle (Vulkan: `rhi::ITexture*`). Null when invalid. */
    void* GetNativeHandle() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    friend class Texture2DAccess;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
