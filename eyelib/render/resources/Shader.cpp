#include "eyelib/render/resources/Shader.h"
#include "eyelib/render/rhi/RHIContext.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/render/PipelineCache.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"

namespace eyelib {
namespace render {

Shader::Shader() = default;

rhi::IDevice* Shader::GetDevice() const {
    return m_injectedDevice ? m_injectedDevice : (m_deviceRHI ? m_deviceRHI : rhi::GetDefaultDevice());
}

Shader::~Shader() {
    Destroy();
}

Shader::Shader(Shader&& other) noexcept {
    m_injectedDevice = other.m_injectedDevice;
    m_deviceRHI = other.m_deviceRHI;
    m_pipelineRHI = other.m_pipelineRHI;
    other.m_injectedDevice = nullptr;
    other.m_deviceRHI = nullptr;
    other.m_pipelineRHI = nullptr;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this == &other) return *this;
    Destroy();
    m_injectedDevice = other.m_injectedDevice;
    m_deviceRHI = other.m_deviceRHI;
    m_pipelineRHI = other.m_pipelineRHI;
    other.m_injectedDevice = nullptr;
    other.m_deviceRHI = nullptr;
    other.m_pipelineRHI = nullptr;
    return *this;
}

bool Shader::BuildFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
    rhi::IDevice* device = GetDevice();
    if (!device) return false;
    return BuildFromSource(device, vertexSrc, fragmentSrc, Mesh::GetVertexLayoutRHI(), rhi::PrimitiveTopology::TriangleList);
}

namespace {
// Accepts a vertex shader path (e.g. assets/shaders/opengl/passes/forward/skinned.vert) or logical name (e.g. passes/forward/skinned).
// Returns logical name (e.g. "passes/forward/skinned") by stripping assets/shaders/, backend segment, and extension.
std::string LogicalNameFromPath(const std::string& vertexPath) {
    std::string base = vertexPath;
    const char* prefix = "assets/shaders/";
    const size_t prefixLen = 14;
    bool hadPrefix = (base.size() > prefixLen && base.compare(0, prefixLen, prefix) == 0);
    if (hadPrefix)
        base = base.substr(prefixLen);
    // Only strip first segment (backend name opengl/diligent) when we had the assets/shaders/ prefix
    if (hadPrefix) {
        size_t firstSlash = base.find('/');
        if (firstSlash != std::string::npos)
            base = base.substr(firstSlash + 1);
    }
    auto stripExt = [](std::string& s, const char* ext, size_t len) {
        if (s.size() >= len && s.compare(s.size() - len, len, ext) == 0) s.resize(s.size() - len);
    };
    stripExt(base, ".vert", 5);
    stripExt(base, ".frag", 5);
    stripExt(base, ".vsh", 4);
    stripExt(base, ".psh", 4);
    return base.empty() ? std::string() : base;
}
} // namespace

bool Shader::LoadFromFile(const std::string& vertexPath, const std::string& fragmentPath) {
    rhi::IDevice* device = GetDevice();
    if (device) {
        std::string logicalName = LogicalNameFromPath(vertexPath);
        if (!logicalName.empty() && Load(device, logicalName, Mesh::GetVertexLayoutRHI()))
            return true;
    }
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
    m_reflection = ParseShaderReflection(vertexSrc, fragmentSrc);
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
    rhi::IShader* vs = nullptr;
    rhi::IShader* fs = nullptr;
    if (device->TryLoadShaderFromPaths(vertexPath.c_str(), fragmentPath.c_str(), vs, fs) && vs && fs) {
        if (m_deviceRHI) Destroy(m_deviceRHI);
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
    std::string vertexSrc;
    if (!LoadShaderSource(vertexPath, vertexSrc)) return false;
    std::string fragmentSrc;
    if (!LoadShaderSource(fragmentPath, fragmentSrc)) return false;
    return BuildFromSource(device, vertexSrc, fragmentSrc, layout, topology);
}

bool Shader::Load(rhi::IDevice* device, const std::string& shaderName, const rhi::VertexLayout& layout,
                  rhi::PrimitiveTopology topology) {
    return Load(device, shaderName, layout, topology, {});
}

bool Shader::Reload() {
    if (!m_lastLoadByName || m_lastShaderName.empty()) return false;
    rhi::IDevice* device = GetDevice();
    if (!device) return false;
    return Load(device, m_lastShaderName, m_lastLayout, m_lastTopology, m_lastKeywords);
}

static std::string ApplyDefinesToSource(const std::string& source, const std::vector<std::string>& keywords) {
    if (keywords.empty()) return source;
    std::string prefix;
    for (const auto& k : keywords) {
        if (k.empty()) continue;
        prefix += "#define ";
        prefix += k;
        prefix += " 1\n";
    }
    return prefix + source;
}

bool Shader::Load(rhi::IDevice* device, const std::string& shaderName, const rhi::VertexLayout& layout,
                  rhi::PrimitiveTopology topology, const std::vector<std::string>& keywords) {
    if (!device || shaderName.empty()) return false;
    m_lastLoadByName = true;
    m_lastShaderName = shaderName;
    m_lastLayout = layout;
    m_lastTopology = topology;
    m_lastKeywords = keywords;
    if (keywords.empty()) {
        if (m_deviceRHI) Destroy(m_deviceRHI);
        rhi::IPipeline* pipe = device->CreateGraphicsPipelineFromName(shaderName.c_str(), layout, topology);
        if (!pipe) return false;
        m_pipelineRHI = pipe;
        m_deviceRHI = device;
        return true;
    }
    std::string key = PipelineCacheKey(shaderName, topology, keywords);
    uint32_t format = 0;
    std::vector<uint8_t> cached;
    if (LoadPipelineBinary(key, format, cached) && !cached.empty()) {
        rhi::IPipeline* pipe = device->CreateGraphicsPipelineFromBinary(cached.data(), cached.size(), format, layout, topology);
        if (pipe) {
            if (m_deviceRHI) Destroy(m_deviceRHI);
            m_pipelineRHI = pipe;
            m_deviceRHI = device;
            return true;
        }
    }
    const char* backend = device->GetShaderBackendName();
    const char* vertExt = device->GetShaderVertexExtension();
    const char* fragExt = device->GetShaderFragmentExtension();
    if (!backend || !vertExt || !fragExt) {
        core::Log::Error("Shader::Load with keywords: device did not provide shader path info");
        return false;
    }
    std::string basePath = std::string("assets/shaders/") + backend + "/" + shaderName;
    std::string vertexPath = basePath + vertExt;
    std::string fragmentPath = basePath + fragExt;
    std::string vertexSrc, fragmentSrc;
    if (!LoadShaderSource(vertexPath, vertexSrc) || !LoadShaderSource(fragmentPath, fragmentSrc)) {
        core::Log::ErrorF("Shader::Load with keywords: failed to load %s", shaderName.c_str());
        return false;
    }
    vertexSrc = ApplyDefinesToSource(vertexSrc, keywords);
    fragmentSrc = ApplyDefinesToSource(fragmentSrc, keywords);
    if (!BuildFromSource(device, vertexSrc, fragmentSrc, layout, topology))
        return false;
    if (m_pipelineRHI) {
        std::vector<uint8_t> bin;
        if (device->GetPipelineBinary(m_pipelineRHI, bin, format) && !bin.empty())
            SavePipelineBinary(key, format, bin.data(), bin.size());
    }
    return true;
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
    rhi::IDevice* device = GetDevice();
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
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformInt(name.c_str(), value);
}

void Shader::SetFloat(const std::string& name, float value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformFloat(name.c_str(), value);
}

void Shader::SetVec2(const std::string& name, const math::Vec2& value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformVec2(name.c_str(), &value.x);
}

void Shader::SetVec3(const std::string& name, const math::Vec3& value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformVec3(name.c_str(), &value.x);
}

void Shader::SetVec4(const std::string& name, const math::Vec4& value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformVec4(name.c_str(), &value.x);
}

void Shader::SetMat4(const std::string& name, const math::Mat4& value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformMat4(name.c_str(), &value[0][0]);
}

void Shader::SetMat4Array(const std::string& name, const math::Mat4* values, size_t count) const {
    if (!values || count == 0) return;
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformMat4Array(name.c_str(), &values[0][0][0], count);
}

void Shader::SetHandle64(const std::string& name, uint64_t value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetImmediateCommandList();
    if (cmd) cmd->SetUniformHandle64(name.c_str(), value);
}

} // namespace render
} // namespace eyelib

