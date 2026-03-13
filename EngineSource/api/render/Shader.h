#pragma once

#include "api/render/RenderDevice.h"
#include "eyelib/math/Types.h"
#include <cstddef>
#include <memory>
#include <string>

namespace eyelib {
namespace render { class Shader; }
namespace api {

class Shader {
public:
    Shader();
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    bool BuildFromSource(RenderDevice device, const std::string& vertexSrc, const std::string& fragmentSrc);
    /** Load shader by logical name (e.g. passes/forward/forward). Single path: resolve backend and create pipeline. */
    bool Load(RenderDevice device, const std::string& logicalName);
    bool LoadFromFile(RenderDevice device, const std::string& vertexPath, const std::string& fragmentPath);
    void Destroy();

    /** Bind pipeline. Device must be non-null. */
    void Bind(RenderDevice device) const;
    void Unbind() const;

    void SetBool(const std::string& name, bool value) const;
    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec2(const std::string& name, const math::Vec2& value) const;
    void SetVec3(const std::string& name, const math::Vec3& value) const;
    void SetVec4(const std::string& name, const math::Vec4& value) const;
    void SetMat4(const std::string& name, const math::Mat4& value) const;
    void SetMat4Array(const std::string& name, const math::Mat4* values, size_t count) const;

    bool IsValid() const;

    std::shared_ptr<::eyelib::render::Shader> GetInternal() const;
    static std::shared_ptr<Shader> WrapInternal(const std::shared_ptr<::eyelib::render::Shader>& shader);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
