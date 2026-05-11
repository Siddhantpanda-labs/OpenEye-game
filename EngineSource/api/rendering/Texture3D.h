#pragma once

#include <cstdint>
#include <memory>

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Public PIMPL handle to a 3D GPU texture (volume).
 *
 * Opaque to clients: the underlying internal type (`eyelib::rendering::Texture3D`) is
 * never named in this header. Construction deferred to **M3** — see `Mesh.h` docstring.
 *
 * Used by:
 *  - Volumetrics / fog (parity slot — no v1 client; M4 player may need it).
 *  - LUTs in compute passes.
 */
class Texture3D {
public:
    Texture3D() noexcept;
    ~Texture3D();

    Texture3D(const Texture3D& other);
    Texture3D& operator=(const Texture3D& other);
    Texture3D(Texture3D&& other) noexcept;
    Texture3D& operator=(Texture3D&& other) noexcept;

    bool IsValid() const noexcept;

    int GetWidth() const noexcept;
    int GetHeight() const noexcept;
    int GetDepth() const noexcept;

    /** Backend-native handle (Vulkan: `rhi::ITexture*`). Null when invalid. */
    void* GetNativeHandle() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    friend class Texture3DAccess;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
