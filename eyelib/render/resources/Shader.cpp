#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/rhi/RHIContext.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <sstream>
#include <unordered_set>

namespace eyelib {
namespace render {

Shader::Shader() = default;

Shader::~Shader() {
    Destroy();
}

Shader::Shader(Shader&& other) noexcept {
    m_deviceRHI = other.m_deviceRHI;
    m_pipelineRHI = other.m_pipelineRHI;
    other.m_deviceRHI = nullptr;
    other.m_pipelineRHI = nullptr;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this == &other) return *this;
    Destroy();
    m_deviceRHI = other.m_deviceRHI;
    m_pipelineRHI = other.m_pipelineRHI;
    other.m_deviceRHI = nullptr;
    other.m_pipelineRHI = nullptr;
    return *this;
}

namespace {
bool AppendShaderSource(const std::string& path,
                        std::string& out,
                        std::unordered_set<std::string>& includeStack,
                        int depth) {
    if (depth > 32) {
        core::Log::ErrorF("Shader include depth exceeded: %s", path.c_str());
        return false;
    }

    const std::string normalized = platform::filesystem::NormalizePath(path);
    if (includeStack.count(normalized) != 0) {
        core::Log::ErrorF("Shader include cycle detected: %s", normalized.c_str());
        return false;
    }

    std::vector<uint8_t> data = platform::filesystem::ReadFile(normalized);
    if (data.empty()) {
        core::Log::ErrorF("Shader include not found: %s", normalized.c_str());
        return false;
    }

    includeStack.insert(normalized);
    std::string src(reinterpret_cast<const char*>(data.data()), data.size());
    std::istringstream stream(src);
    std::string line;
    const std::string baseDir = platform::filesystem::GetDirectory(normalized);

    while (std::getline(stream, line)) {
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        if (trimmed.rfind("#include", 0) == 0) {
            const size_t firstQuote = trimmed.find_first_of("\"<");
            const size_t lastQuote = trimmed.find_last_of("\">");
            if (firstQuote != std::string::npos && lastQuote != std::string::npos && lastQuote > firstQuote + 1) {
                const std::string includeFile = trimmed.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                const std::string includePath = platform::filesystem::NormalizePath(baseDir + "/" + includeFile);
                if (!AppendShaderSource(includePath, out, includeStack, depth + 1)) {
                    includeStack.erase(normalized);
                    return false;
                }
                continue;
            }
        }

        out.append(line);
        out.push_back('\n');
    }

    includeStack.erase(normalized);
    return true;
}

bool LoadShaderSource(const std::string& path, std::string& out) {
    std::unordered_set<std::string> includeStack;
    out.clear();
    return AppendShaderSource(path, out, includeStack, 0);
}
} // namespace

bool Shader::BuildFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    rhi::IDevice* device = rhi::GetDefaultDevice();
    if (!device) return false;
    return BuildFromSource(device, vertexSrc, fragmentSrc, Mesh::GetVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList);
}

bool Shader::LoadFromFile(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertexSrc;
    if (!LoadShaderSource(vertexPath, vertexSrc)) {
        core::Log::ErrorF("Failed to load vertex shader: %s", vertexPath.c_str());
        return false;
    }

    std::string fragmentSrc;
    if (!LoadShaderSource(fragmentPath, fragmentSrc)) {
        core::Log::ErrorF("Failed to load fragment shader: %s", fragmentPath.c_str());
        return false;
    }

    return BuildFromSource(vertexSrc, fragmentSrc);
}

bool Shader::BuildFromSource(rhi::IDevice* device, const std::string& vertexSrc, const std::string& fragmentSrc, const rhi::VertexLayout& layout) {
    return BuildFromSource(device, vertexSrc, fragmentSrc, layout, rhi::PrimitiveTopology::TriangleList);
}

bool Shader::BuildFromSource(rhi::IDevice* device, const std::string& vertexSrc, const std::string& fragmentSrc,
                             const rhi::VertexLayout& layout, rhi::PrimitiveTopology topology) {
    if (!device) return false;
    if (m_deviceRHI) Destroy(m_deviceRHI);
    rhi::IShader* vs = device->CreateShader(rhi::ShaderStage::Vertex, vertexSrc.c_str(), vertexSrc.size());
    rhi::IShader* fs = device->CreateShader(rhi::ShaderStage::Fragment, fragmentSrc.c_str(), fragmentSrc.size());
    if (!vs || !fs) {
        if (vs) device->DestroyShader(vs);
        if (fs) device->DestroyShader(fs);
        return false;
    }
    rhi::PipelineDesc desc;
    desc.vertexShader = vs;
    desc.fragmentShader = fs;
    desc.vertexLayout = layout;
    desc.topology = topology;
    desc.depthStencil.depthTest = true;
    desc.depthStencil.depthWrite = true;
    rhi::IPipeline* pipe = device->CreateGraphicsPipeline(desc);
    device->DestroyShader(vs);
    device->DestroyShader(fs);
    if (!pipe) return false;
    m_pipelineRHI = pipe;
    m_deviceRHI = device;
    return true;
}

bool Shader::LoadFromFile(rhi::IDevice* device, const std::string& vertexPath, const std::string& fragmentPath, const rhi::VertexLayout& layout) {
    return LoadFromFile(device, vertexPath, fragmentPath, layout, rhi::PrimitiveTopology::TriangleList);
}

bool Shader::LoadFromFile(rhi::IDevice* device, const std::string& vertexPath, const std::string& fragmentPath,
                          const rhi::VertexLayout& layout, rhi::PrimitiveTopology topology) {
    if (!device) return false;
    std::string vertexSrc;
    if (!LoadShaderSource(vertexPath, vertexSrc)) return false;
    std::string fragmentSrc;
    if (!LoadShaderSource(fragmentPath, fragmentSrc)) return false;
    return BuildFromSource(device, vertexSrc, fragmentSrc, layout, topology);
}

void Shader::Destroy() {
    if (m_deviceRHI) Destroy(m_deviceRHI);
}

void Shader::Destroy(rhi::IDevice* device) {
    if (!device || device != m_deviceRHI) return;
    if (m_pipelineRHI) {
        device->DestroyPipeline(m_pipelineRHI);
        m_pipelineRHI = nullptr;
    }
    m_deviceRHI = nullptr;
}

void Shader::Bind() const {
    rhi::IDevice* device = m_deviceRHI ? m_deviceRHI : rhi::GetDefaultDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->BindPipeline(m_pipelineRHI);
}

void Shader::Unbind() const {
    (void)this;
}

void Shader::SetBool(const std::string& name, bool value) const {
    SetInt(name, value ? 1 : 0);
}

void Shader::SetInt(const std::string& name, int value) const {
    rhi::IDevice* device = m_deviceRHI ? m_deviceRHI : rhi::GetDefaultDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformInt(name.c_str(), value);
}

void Shader::SetFloat(const std::string& name, float value) const {
    rhi::IDevice* device = m_deviceRHI ? m_deviceRHI : rhi::GetDefaultDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformFloat(name.c_str(), value);
}

void Shader::SetVec2(const std::string& name, const math::Vec2& value) const {
    rhi::IDevice* device = m_deviceRHI ? m_deviceRHI : rhi::GetDefaultDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformVec2(name.c_str(), &value.x);
}

void Shader::SetVec3(const std::string& name, const math::Vec3& value) const {
    rhi::IDevice* device = m_deviceRHI ? m_deviceRHI : rhi::GetDefaultDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformVec3(name.c_str(), &value.x);
}

void Shader::SetVec4(const std::string& name, const math::Vec4& value) const {
    rhi::IDevice* device = m_deviceRHI ? m_deviceRHI : rhi::GetDefaultDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformVec4(name.c_str(), &value.x);
}

void Shader::SetMat4(const std::string& name, const math::Mat4& value) const {
    rhi::IDevice* device = m_deviceRHI ? m_deviceRHI : rhi::GetDefaultDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformMat4(name.c_str(), &value[0][0]);
}

void Shader::SetMat4Array(const std::string& name, const math::Mat4* values, size_t count) const {
    if (!values || count == 0) return;
    rhi::IDevice* device = m_deviceRHI ? m_deviceRHI : rhi::GetDefaultDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformMat4Array(name.c_str(), &values[0][0][0], count);
}

} // namespace render
} // namespace eyelib

