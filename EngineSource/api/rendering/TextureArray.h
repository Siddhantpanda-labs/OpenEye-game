#pragma once

#include <cstdint>
#include <memory>

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Public PIMPL handle to a 2D-array GPU texture (N layers of WxH).
 *
 * Opaque to clients: the underlying internal type (`eyelib::rendering::TextureArray`) is
 * never named in this header. Construction deferred to **M3** — see `Mesh.h` docstring.
 *
 * Used by:
 *  - Texture atlases (UI / impostors).
 *  - Spot-light shadow atlas debug surfacing (M3.7 — shadow debug panel may consume).
 */
class TextureArray {
public:
    TextureArray() noexcept;
    ~TextureArray();

    TextureArray(const TextureArray& other);
    TextureArray& operator=(const TextureArray& other);
    TextureArray(TextureArray&& other) noexcept;
    TextureArray& operator=(TextureArray&& other) noexcept;

    bool IsValid() const noexcept;

    int GetWidth() const noexcept;
    int GetHeight() const noexcept;
    int GetLayerCount() const noexcept;

    /** Backend-native handle (Vulkan: `rhi::ITexture*`). Null when invalid. */
    void* GetNativeHandle() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    friend class TextureArrayAccess;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
