#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace eyelib {
namespace api {
namespace rendering {

/**
 * @brief Public PIMPL handle to a compiled GPU shader / pipeline-template descriptor.
 *
 * Opaque to clients: the underlying internal type (`eyelib::rendering::Shader`) is never
 * named in this header. Construction deferred to **M3** — see `Mesh.h` docstring.
 *
 * In the new module, "shader" usually means a `MaterialShader` template owned by the
 * `ShaderLibrary` inside `RasterFeature`; clients rarely hold a direct shader handle.
 * This façade exists primarily so the editor's `EditorGPUResourceCache` keeps a typed
 * shared map of `AssetId -> Shader` post-migration without reaching for internal types.
 */
class Shader {
public:
    Shader() noexcept;
    ~Shader();

    Shader(const Shader& other);
    Shader& operator=(const Shader& other);
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool IsValid() const noexcept;

    /** Friendly debug name for the shader / pipeline template. Empty when invalid. */
    std::string GetName() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;

    friend class ShaderAccess;
};

} // namespace rendering
} // namespace api
} // namespace eyelib
