#include "api/render/Shader.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/resources/Shader.h"

namespace eyelib {
namespace api {

class Shader::Impl {
public:
    std::shared_ptr<::eyelib::render::Shader> shader = std::make_shared<::eyelib::render::Shader>();
};

Shader::Shader() : m_impl(std::make_unique<Impl>()) {}

Shader::~Shader() = default;

Shader::Shader(Shader&& other) noexcept = default;

Shader& Shader::operator=(Shader&& other) noexcept = default;

bool Shader::BuildFromSource(RenderDevice device, const std::string& vertexSrc, const std::string& fragmentSrc) {
    if (!device) return false;
    return m_impl->shader->BuildFromSource(
        static_cast<::eyelib::render::rhi::IDevice*>(device.Get()),
        vertexSrc, fragmentSrc, ::eyelib::render::Mesh::GetVertexLayoutRHI());
}

bool Shader::Load(RenderDevice device, const std::string& logicalName) {
    if (!device || logicalName.empty()) return false;
    auto* rhiDevice = static_cast<::eyelib::render::rhi::IDevice*>(device.Get());
    return m_impl->shader->Load(rhiDevice, logicalName, ::eyelib::render::Mesh::GetVertexLayoutRHI());
}

bool Shader::LoadFromFile(RenderDevice device, const std::string& vertexPath, const std::string& fragmentPath) {
    if (!device) return false;
    return m_impl->shader->LoadFromFile(
        static_cast<::eyelib::render::rhi::IDevice*>(device.Get()),
        vertexPath, fragmentPath, ::eyelib::render::Mesh::GetVertexLayoutRHI());
}

void Shader::Destroy() {
    m_impl->shader->Destroy();
}

void Shader::Bind(RenderDevice device) const {
    if (device) m_impl->shader->Bind(static_cast<::eyelib::render::rhi::IDevice*>(device.Get()));
}

void Shader::Unbind() const {
    m_impl->shader->Unbind();
}

void Shader::SetBool(const std::string& name, bool value) const {
    m_impl->shader->SetBool(name, value);
}

void Shader::SetInt(const std::string& name, int value) const {
    m_impl->shader->SetInt(name, value);
}

void Shader::SetFloat(const std::string& name, float value) const {
    m_impl->shader->SetFloat(name, value);
}

void Shader::SetVec2(const std::string& name, const math::Vec2& value) const {
    m_impl->shader->SetVec2(name, value);
}

void Shader::SetVec3(const std::string& name, const math::Vec3& value) const {
    m_impl->shader->SetVec3(name, value);
}

void Shader::SetVec4(const std::string& name, const math::Vec4& value) const {
    m_impl->shader->SetVec4(name, value);
}

void Shader::SetMat4(const std::string& name, const math::Mat4& value) const {
    m_impl->shader->SetMat4(name, value);
}

void Shader::SetMat4Array(const std::string& name, const math::Mat4* values, size_t count) const {
    m_impl->shader->SetMat4Array(name, values, count);
}

bool Shader::IsValid() const {
    return m_impl->shader->IsValid();
}

std::shared_ptr<::eyelib::render::Shader> Shader::GetInternal() const {
    return m_impl->shader;
}

std::shared_ptr<Shader> Shader::WrapInternal(const std::shared_ptr<::eyelib::render::Shader>& shader) {
    auto apiShader = std::make_shared<Shader>();
    if (shader) apiShader->m_impl->shader = shader;
    return apiShader;
}

} // namespace api
} // namespace eyelib
