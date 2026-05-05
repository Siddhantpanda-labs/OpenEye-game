#include "eyelib/render/resources/Shader.h"
#include <cstring>
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/ShaderPathResolve.h"
#include "eyelib/render/ShaderSourceLoader.h"
#include "eyelib/render/PipelineCache.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"

namespace eyelib {
namespace render {

namespace {
bool VulkanDisablesLegacyUniforms(rhi::IDevice* device) {
    if (!device) return false;
    const char* b = device->GetShaderBackendName();
    return b && std::strcmp(b, "vulkan") == 0;
}
} // namespace

Shader::Shader() = default;

Shader::~Shader() {
    Destroy();
}

Shader::Shader(Shader&& other) noexcept {
    m_deviceRHI = other.m_deviceRHI;
    m_pipelineRHI = other.m_pipelineRHI;
    m_reflection = std::move(other.m_reflection);
    m_descriptor = std::move(other.m_descriptor);
    other.m_deviceRHI = nullptr;
    other.m_pipelineRHI = nullptr;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this == &other) return *this;
    Destroy();
    m_deviceRHI = other.m_deviceRHI;
    m_pipelineRHI = other.m_pipelineRHI;
    m_reflection = std::move(other.m_reflection);
    m_descriptor = std::move(other.m_descriptor);
    other.m_deviceRHI = nullptr;
    other.m_pipelineRHI = nullptr;
    return *this;
}

bool Shader::BuildFromSource(rhi::IDevice* device, const std::string& vertexSrc, const std::string& fragmentSrc, const rhi::VertexLayout& layout) {
    return BuildFromSource(device, vertexSrc, fragmentSrc, layout, rhi::PrimitiveTopology::TriangleList, rhi::PipelineLayoutKind::PushOnly, std::nullopt);
}

bool Shader::BuildFromSource(rhi::IDevice* device, const std::string& vertexSrc, const std::string& fragmentSrc,
                             const rhi::VertexLayout& layout, rhi::PrimitiveTopology topology,
                             rhi::PipelineLayoutKind layoutKind,
                             const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats) {
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
    desc.layoutKind = layoutKind;
    desc.attachmentFormats = attachmentFormats;
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

bool Shader::BuildFromShaders(rhi::IDevice* device, rhi::IShader* vs, rhi::IShader* fs, const rhi::VertexLayout& layout,
                             rhi::PrimitiveTopology topology, rhi::PipelineLayoutKind layoutKind,
                             const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats) {
    if (!device || !vs || !fs) return false;
    if (m_deviceRHI) Destroy(m_deviceRHI);
    rhi::PipelineDesc desc;
    desc.vertexShader = vs;
    desc.fragmentShader = fs;
    desc.vertexLayout = layout;
    desc.topology = topology;
    desc.layoutKind = layoutKind;
    desc.attachmentFormats = attachmentFormats;
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

static bool HasShaderExtension(const std::string& path) {
    const char* exts[] = { ".vert", ".frag", ".vsh", ".psh" };
    for (const char* ext : exts) {
        size_t len = std::strlen(ext);
        if (path.size() >= len && path.compare(path.size() - len, len, ext) == 0)
            return true;
    }
    return false;
}

bool Shader::LoadFromFile(rhi::IDevice* device, const std::string& vertexPath, const std::string& fragmentPath,
                          const rhi::VertexLayout& layout, rhi::PrimitiveTopology topology,
                          const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats) {
    if (!device) return false;
    if (vertexPath == fragmentPath && !HasShaderExtension(vertexPath)) {
        std::string logicalName = ShaderPathToLogicalName(vertexPath);
        if (!logicalName.empty() && Load(device, logicalName, layout, topology, rhi::PipelineLayoutKind::PushOnly, attachmentFormats))
            return true;
    }
    m_descriptor = {};
    m_descriptor.loadFromAsset = false;
    m_descriptor.vertexPath = vertexPath;
    m_descriptor.fragmentPath = fragmentPath;
    m_descriptor.layout = layout;
    m_descriptor.topology = topology;
    m_descriptor.attachmentFormats = attachmentFormats;
    rhi::IShader* vs = nullptr;
    rhi::IShader* fs = nullptr;
    const char* backend = device->GetShaderBackendName();
    const bool trySpv = backend && std::strcmp(backend, "vulkan") == 0;
    if (trySpv) {
        const std::string vertSpv = std::string(vertexPath) + ".spv";
        const std::string fragSpv = std::string(fragmentPath) + ".spv";
        if (device->TryLoadShaderFromPaths(vertSpv.c_str(), fragSpv.c_str(), vs, fs) && vs && fs)
            goto create_pipeline_from_shaders;
    }
    if (device->TryLoadShaderFromPaths(vertexPath.c_str(), fragmentPath.c_str(), vs, fs) && vs && fs) {
create_pipeline_from_shaders:
        if (m_deviceRHI) Destroy(m_deviceRHI);
        rhi::PipelineDesc desc;
        desc.vertexShader = vs;
        desc.fragmentShader = fs;
        desc.vertexLayout = layout;
        desc.topology = topology;
        desc.attachmentFormats = attachmentFormats;
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
    return BuildFromSource(device, vertexSrc, fragmentSrc, layout, topology, rhi::PipelineLayoutKind::PushOnly, attachmentFormats);
}

bool Shader::Load(rhi::IDevice* device, const std::string& assetPath, const rhi::VertexLayout& layout,
                  rhi::PrimitiveTopology topology, rhi::PipelineLayoutKind layoutKind,
                  const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats) {
    return Load(device, assetPath, layout, topology, {}, nullptr, layoutKind, attachmentFormats);
}

bool Shader::Load(rhi::IDevice* device, const std::string& assetPath, const rhi::VertexLayout& layout,
                  rhi::PrimitiveTopology topology, const rhi::DepthStencilState* depthStencilOverride,
                  rhi::PipelineLayoutKind layoutKind,
                  const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats) {
    return Load(device, assetPath, layout, topology, {}, depthStencilOverride, layoutKind, attachmentFormats);
}

bool Shader::Reload() {
    bool canReload = (m_descriptor.loadFromAsset && !m_descriptor.assetPath.empty()) ||
                     !m_descriptor.vertexPath.empty() || !m_descriptor.fragmentPath.empty();
    if (!canReload) return false;
    rhi::IDevice* device = GetDevice();
    if (!device) return false;
    return LoadFromDescriptor(device, m_descriptor);
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

bool Shader::Load(rhi::IDevice* device, const std::string& assetPath, const rhi::VertexLayout& layout,
                  rhi::PrimitiveTopology topology, const std::vector<std::string>& keywords,
                  const rhi::DepthStencilState* depthStencilOverride, rhi::PipelineLayoutKind layoutKind,
                  const std::optional<rhi::PipelineAttachmentFormats>& attachmentFormats) {
    if (!device || assetPath.empty()) return false;
    m_descriptor = {};
    m_descriptor.loadFromAsset = true;
    m_descriptor.assetPath = assetPath;
    m_descriptor.layout = layout;
    m_descriptor.topology = topology;
    m_descriptor.keywords = keywords;
    m_descriptor.layoutKind = layoutKind;
    m_descriptor.attachmentFormats = attachmentFormats;
    if (keywords.empty()) {
        if (m_deviceRHI) Destroy(m_deviceRHI);
        rhi::ShaderGraphicsPipelineCreateInfo gp{};
        gp.assetPath = assetPath;
        gp.vertexLayout = layout;
        gp.topology = topology;
        gp.layoutKind = layoutKind;
        gp.attachmentFormats = attachmentFormats;
        gp.sourcePolicy = rhi::ShaderSourcePolicy::PreferSpirv;
        if (depthStencilOverride) {
            gp.useDepthStencilOverride = true;
            gp.depthStencilOverride = *depthStencilOverride;
        }
        rhi::IPipeline* pipe = device->CreateGraphicsPipelineFromShaderAsset(gp);
        if (!pipe) return false;
        m_pipelineRHI = pipe;
        m_deviceRHI = device;
        return true;
    }
    std::string key = PipelineCacheKey(assetPath, topology, keywords);
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
    std::string root = platform::filesystem::GetEngineAssetsRoot();
    std::string basePath = root.empty()
        ? (std::string("assets/shaders/") + backend + "/" + assetPath)
        : platform::filesystem::NormalizePath(root + "/assets/shaders/" + backend + "/" + assetPath);
    std::string vertexPath = basePath + vertExt;
    std::string fragmentPath = basePath + fragExt;
    std::string vertexSrc, fragmentSrc;
    if (!LoadShaderSource(vertexPath, vertexSrc) || !LoadShaderSource(fragmentPath, fragmentSrc)) {
        core::Log::ErrorF("Shader::Load with keywords: failed to load %s", assetPath.c_str());
        return false;
    }
    vertexSrc = ApplyDefinesToSource(vertexSrc, keywords);
    fragmentSrc = ApplyDefinesToSource(fragmentSrc, keywords);
    m_reflection = ParseShaderReflection(vertexSrc, fragmentSrc);
    if (m_deviceRHI) Destroy(m_deviceRHI);
    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    gp.assetPath = assetPath;
    gp.vertexLayout = layout;
    gp.topology = topology;
    gp.layoutKind = layoutKind;
    gp.attachmentFormats = attachmentFormats;
    gp.vertexSourceOverride = std::move(vertexSrc);
    gp.fragmentSourceOverride = std::move(fragmentSrc);
    if (depthStencilOverride) {
        gp.useDepthStencilOverride = true;
        gp.depthStencilOverride = *depthStencilOverride;
    }
    rhi::IPipeline* pipe = device->CreateGraphicsPipelineFromShaderAsset(gp);
    if (!pipe) return false;
    m_pipelineRHI = pipe;
    m_deviceRHI = device;
    if (m_pipelineRHI) {
        std::vector<uint8_t> bin;
        if (device->GetPipelineBinary(m_pipelineRHI, bin, format) && !bin.empty())
            SavePipelineBinary(key, format, bin.data(), bin.size());
    }
    return true;
}

ShaderDescriptor Shader::GetDescriptor() const {
    return m_descriptor;
}

bool Shader::LoadFromDescriptor(rhi::IDevice* device, const ShaderDescriptor& desc) {
    if (!device) return false;
    if (desc.loadFromAsset && !desc.assetPath.empty())
        return Load(device, desc.assetPath, desc.layout, desc.topology, desc.keywords, nullptr, desc.layoutKind, desc.attachmentFormats);
    if (!desc.vertexPath.empty() || !desc.fragmentPath.empty())
        return LoadFromFile(device, desc.vertexPath, desc.fragmentPath, desc.layout, desc.topology, desc.attachmentFormats);
    return false;
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

void Shader::Bind(rhi::IDevice* device) const {
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
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
    if (VulkanDisablesLegacyUniforms(device)) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (cmd) cmd->SetUniformInt(name.c_str(), value);
}

void Shader::SetUint(const std::string& name, uint32_t value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    if (VulkanDisablesLegacyUniforms(device)) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (cmd) cmd->SetUniformUint(name.c_str(), value);
}

void Shader::SetFloat(const std::string& name, float value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (cmd) cmd->SetUniformFloat(name.c_str(), value);
}

void Shader::SetVec2(const std::string& name, const math::Vec2& value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    if (VulkanDisablesLegacyUniforms(device)) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (cmd) cmd->SetUniformVec2(name.c_str(), &value.x);
}

void Shader::SetVec3(const std::string& name, const math::Vec3& value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    if (VulkanDisablesLegacyUniforms(device)) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (cmd) cmd->SetUniformVec3(name.c_str(), &value.x);
}

void Shader::SetVec4(const std::string& name, const math::Vec4& value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    if (VulkanDisablesLegacyUniforms(device)) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (cmd) cmd->SetUniformVec4(name.c_str(), &value.x);
}

void Shader::SetMat4(const std::string& name, const math::Mat4& value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    if (VulkanDisablesLegacyUniforms(device)) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (cmd) cmd->SetUniformMat4(name.c_str(), &value[0][0]);
}

void Shader::SetMat4Array(const std::string& name, const math::Mat4* values, size_t count) const {
    if (!values || count == 0) return;
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    if (VulkanDisablesLegacyUniforms(device)) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (cmd) cmd->SetUniformMat4Array(name.c_str(), &values[0][0][0], count);
}

void Shader::SetHandle64(const std::string& name, uint64_t value) const {
    rhi::IDevice* device = GetDevice();
    if (!device || !m_pipelineRHI) return;
    if (VulkanDisablesLegacyUniforms(device)) return;
    rhi::ICommandList* cmd = device->GetActiveRecordingCommandList();
    if (cmd) cmd->SetUniformHandle64(name.c_str(), value);
}

} // namespace render
} // namespace eyelib

