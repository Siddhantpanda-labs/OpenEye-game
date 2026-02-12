#include "eyelib/render/rhi/bgfx/DeviceBGFX.h"
#include "eyelib/render/rhi/bgfx/BufferBGFX.h"
#include "eyelib/render/rhi/bgfx/TextureBGFX.h"
#include "eyelib/render/rhi/bgfx/ShaderBGFX.h"
#include "eyelib/render/rhi/bgfx/PipelineBGFX.h"
#include "eyelib/render/rhi/bgfx/RenderTargetBGFX.h"
#include "eyelib/render/rhi/bgfx/QueryPoolBGFX.h"
#include "eyelib/render/rhi/bgfx/BGFXEnums.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include "eyelib/core/Log.h"
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#if defined(_MSC_VER) && defined(_WIN32)
#include <excpt.h>
#endif

namespace eyelib {
namespace render {
namespace rhi {

DeviceBGFX::DeviceBGFX(void* nwh, uint32_t width, uint32_t height)
    : m_width(width), m_height(height) {
    core::Log::InfoF("DeviceBGFX::DeviceBGFX: nwh=%p %ux%u", nwh, width, height);

    bgfx::Init init;
    init.type = bgfx::RendererType::Vulkan;  // use Vulkan backend
    init.vendorId = BGFX_PCI_ID_NONE;
    init.deviceId = 0;
    init.resolution.width = static_cast<uint16_t>(width);
    init.resolution.height = static_cast<uint16_t>(height);
    init.resolution.reset = BGFX_RESET_VSYNC;
    init.platformData.nwh = nwh;
    init.platformData.context = nullptr;
    init.platformData.backBuffer = nullptr;
    init.platformData.backBufferDS = nullptr;

    if (!bgfx::init(init)) {
        core::Log::Warn("DeviceBGFX: Vulkan init failed, trying auto (Count)");
        init.type = bgfx::RendererType::Count;
        if (!bgfx::init(init)) {
            core::Log::Warn("DeviceBGFX: auto init failed, trying OpenGL and D3D11 fallbacks");
            bgfx::RendererType::Enum fallback[] = {
                bgfx::RendererType::OpenGL,
                bgfx::RendererType::Direct3D11,
            };
            for (auto t : fallback) {
                init.type = t;
                if (bgfx::init(init)) {
                    core::Log::InfoF("DeviceBGFX: bgfx::init succeeded with renderer type %d", static_cast<int>(t));
                    break;
                }
            }
        } else {
            core::Log::Info("DeviceBGFX: bgfx::init succeeded (auto)");
        }
    } else {
        core::Log::Info("DeviceBGFX: bgfx::init succeeded (Vulkan)");
    }

    const bgfx::RendererType::Enum rt = bgfx::getRendererType();
    core::Log::InfoF("DeviceBGFX: active renderer type = %d", static_cast<int>(rt));
    m_immediateCommandList = std::make_unique<CommandListBGFX>(this);
}

DeviceBGFX::~DeviceBGFX() {
    if (m_blitProgramIdx != kInvalidIdx) {
        bgfx::ProgramHandle h; h.idx = m_blitProgramIdx;
        if (bgfx::isValid(h)) bgfx::destroy(h);
    }
    if (m_fullscreenVbIdx != kInvalidIdx) {
        bgfx::VertexBufferHandle h; h.idx = m_fullscreenVbIdx;
        if (bgfx::isValid(h)) bgfx::destroy(h);
    }
    if (m_fullscreenIbIdx != kInvalidIdx) {
        bgfx::IndexBufferHandle h; h.idx = m_fullscreenIbIdx;
        if (bgfx::isValid(h)) bgfx::destroy(h);
    }
    if (m_blitTextureUniformIdx != kInvalidIdx) {
        bgfx::UniformHandle h; h.idx = m_blitTextureUniformIdx;
        if (bgfx::isValid(h)) bgfx::destroy(h);
    }
    m_immediateCommandList.reset();
    bgfx::shutdown();
}

ICommandList* DeviceBGFX::GetImmediateCommandList() {
    return m_immediateCommandList.get();
}

void DeviceBGFX::SubmitCommandList(ICommandList* list) {
    (void)list;
}

namespace {
    // Map our VertexLayout to BGFX for mesh VBs. Attributes must be added in offset order.
    void buildBGFXVertexLayout(const VertexLayout& src, bgfx::VertexLayout& out) {
        out.begin();
        if (src.attributes.empty()) {
            out.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
            out.end();
            return;
        }
        std::vector<std::pair<uint32_t, const VertexAttribute*>> sorted;
        for (const auto& a : src.attributes)
            sorted.push_back({a.offsetInBytes, &a});
        std::sort(sorted.begin(), sorted.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
        uint32_t prevEnd = 0;
        for (const auto& p : sorted) {
            uint32_t off = p.first;
            const VertexAttribute& a = *p.second;
            if (off > prevEnd)
                out.skip(static_cast<uint8_t>(off - prevEnd));
            bgfx::Attrib::Enum attr = bgfx::Attrib::Position;
            switch (a.location) {
                case 0: attr = bgfx::Attrib::Position; break;
                case 1: attr = bgfx::Attrib::Normal; break;
                case 2: attr = bgfx::Attrib::TexCoord0; break;
                case 3: attr = bgfx::Attrib::Tangent; break;
                case 4: attr = bgfx::Attrib::Color0; break;
                case 5: attr = bgfx::Attrib::Indices; break;
                case 6: attr = bgfx::Attrib::Weight; break;
                case 7: attr = bgfx::Attrib::TexCoord1; break;
                default: continue;
            }
            uint8_t num = 1;
            bgfx::AttribType::Enum type = bgfx::AttribType::Float;
            switch (a.type) {
                case VertexAttributeType::Float:  num = 1; type = bgfx::AttribType::Float; break;
                case VertexAttributeType::Float2: num = 2; type = bgfx::AttribType::Float; break;
                case VertexAttributeType::Float3: num = 3; type = bgfx::AttribType::Float; break;
                case VertexAttributeType::Float4: num = 4; type = bgfx::AttribType::Float; break;
                case VertexAttributeType::Int4:
                case VertexAttributeType::UInt4: num = 4; type = bgfx::AttribType::Float; break; // 16 bytes; shader casts to ivec4
                default: continue;
            }
            out.add(attr, num, type);
            prevEnd = off + static_cast<uint32_t>(num) * (type == bgfx::AttribType::Float ? 4u : 1u);
        }
        out.end();
    }
}

IBuffer* DeviceBGFX::CreateBuffer(const BufferDesc& desc, const void* initialData) {
    if (desc.sizeInBytes == 0) return nullptr;
    auto* b = new BufferBGFX();
    b->m_size = desc.sizeInBytes;
    b->m_hint = desc.memoryHint;

    if (HasUsage(desc.usage, BufferUsage::Uniform)) {
        b->m_uniform = true;
        b->m_cpuData.resize(desc.sizeInBytes);
        if (initialData)
            std::memcpy(b->m_cpuData.data(), initialData, desc.sizeInBytes);
        return b;
    }

    const bgfx::Memory* mem = initialData
        ? bgfx::copy(initialData, static_cast<uint32_t>(desc.sizeInBytes))
        : bgfx::alloc(static_cast<uint32_t>(desc.sizeInBytes));
    if (!mem) { delete b; return nullptr; }

    if (HasUsage(desc.usage, BufferUsage::Index)) {
        b->m_index = true;
        bgfx::IndexBufferHandle ib = bgfx::createIndexBuffer(mem, BGFXEnums::IndexTypeToBGFX(IndexType::UInt32));
        b->m_ibIdx = ib.idx;
    } else {
        b->m_vertex = true;
        bgfx::VertexLayout layout;
        if (desc.vertexLayout && !desc.vertexLayout->attributes.empty()) {
            buildBGFXVertexLayout(*desc.vertexLayout, layout);
        } else {
            layout.begin();
            if (desc.sizeInBytes == 64u) {
                layout.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float).add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float);
            } else {
                layout.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
            }
            layout.end();
        }
        bgfx::VertexBufferHandle vb = bgfx::createVertexBuffer(mem, layout);
        b->m_vbIdx = vb.idx;
    }
    return b;
}

void DeviceBGFX::DestroyBuffer(IBuffer* buffer) {
    auto* b = static_cast<BufferBGFX*>(buffer);
    if (!b) return;
    if (b->m_vertex && b->m_vbIdx != BufferBGFX::kInvalidIdx) {
        bgfx::VertexBufferHandle h; h.idx = b->m_vbIdx;
        bgfx::destroy(h);
    }
    if (b->m_index && b->m_ibIdx != BufferBGFX::kInvalidIdx) {
        bgfx::IndexBufferHandle h; h.idx = b->m_ibIdx;
        bgfx::destroy(h);
    }
    delete b;
}

ITexture* DeviceBGFX::CreateTexture(const TextureDesc& desc, const void* initialData) {
    auto* tex = new TextureBGFX();
    tex->m_width = desc.width;
    tex->m_height = desc.height;
    tex->m_depthOrArray = desc.depthOrArrayLayers;
    tex->m_type = desc.type;
    tex->m_format = desc.format;

    bgfx::TextureFormat::Enum fmt = static_cast<bgfx::TextureFormat::Enum>(BGFXEnums::TextureFormatToBGFX(desc.format));
    uint64_t flags = BGFXEnums::TextureFormatToBGFXSamplerFlags(desc.format);

    if (desc.type == TextureType::Texture2D) {
        const bgfx::Memory* mem = nullptr;
        if (initialData) {
            uint32_t size = desc.width * desc.height * 4;  // rough
            if (desc.format == RHITextureFormat::RGBA8) size = desc.width * desc.height * 4;
            else if (desc.format == RHITextureFormat::RGB8) size = desc.width * desc.height * 3;
            mem = bgfx::copy(initialData, size);
        }
        bgfx::TextureHandle th = bgfx::createTexture2D(
            static_cast<uint16_t>(desc.width),
            static_cast<uint16_t>(desc.height),
            desc.mipLevels > 1,
            1,
            fmt,
            flags,
            mem
        );
        tex->m_texIdx = th.idx;
    } else {
        bgfx::TextureHandle th = bgfx::createTexture2D(
            static_cast<uint16_t>(desc.width),
            static_cast<uint16_t>(desc.height),
            false,
            1,
            fmt,
            flags,
            nullptr
        );
        tex->m_texIdx = th.idx;
    }
    return tex;
}

void DeviceBGFX::DestroyTexture(ITexture* texture) {
    auto* t = static_cast<TextureBGFX*>(texture);
    if (t && t->m_texIdx != TextureBGFX::kInvalidIdx) {
        bgfx::TextureHandle h; h.idx = t->m_texIdx;
        if (bgfx::isValid(h)) bgfx::destroy(h);
    }
    delete t;
}

IShader* DeviceBGFX::CreateShader(ShaderStage stage, const char* source, size_t sourceLen) {
    (void)source;
    (void)sourceLen;
    // BGFX expects precompiled shaders; runtime GLSL not supported. Return invalid handle.
    auto* s = new ShaderBGFX();
    s->m_stage = stage;
    return s;
}

IShader* DeviceBGFX::CreateShaderFromBinary(ShaderStage stage, const void* data, size_t size) {
    if (!data || size == 0) return nullptr;
    const bgfx::Memory* mem = bgfx::copy(data, static_cast<uint32_t>(size));
    if (!mem) return nullptr;
    bgfx::ShaderHandle h = bgfx::createShader(mem);
    if (!bgfx::isValid(h)) return nullptr;
    auto* s = new ShaderBGFX();
    s->m_stage = stage;
    s->m_shaderIdx = h.idx;
    return s;
}

void DeviceBGFX::DestroyShader(IShader* shader) {
    auto* s = static_cast<ShaderBGFX*>(shader);
    if (s && s->m_shaderIdx != ShaderBGFX::kInvalidIdx) {
        bgfx::ShaderHandle h; h.idx = s->m_shaderIdx;
        if (bgfx::isValid(h)) bgfx::destroy(h);
    }
    delete s;
}

bool DeviceBGFX::TryLoadShaderFromPaths(const char* vertexPath, const char* fragmentPath, IShader*& outVs, IShader*& outFs) {
    outVs = nullptr;
    outFs = nullptr;
    if (!vertexPath || !fragmentPath) return false;
    std::string fn = eyelib::platform::filesystem::GetFilename(vertexPath);
    size_t dot = fn.rfind('.');
    std::string name = (dot != std::string::npos) ? fn.substr(0, dot) : fn;
    if (name.empty()) return false;

    const char* rendererDir = "gl";
    switch (bgfx::getRendererType()) {
        case bgfx::RendererType::OpenGL:
        case bgfx::RendererType::OpenGLES: rendererDir = "gl"; break;
        case bgfx::RendererType::Direct3D11: rendererDir = "d3d11"; break;
        case bgfx::RendererType::Vulkan: rendererDir = "vulkan"; break;
        default: rendererDir = "gl"; break;
    }

    std::string baseDir;
    std::string exePath = eyelib::platform::filesystem::GetExecutablePath();
    if (!exePath.empty()) {
        baseDir = eyelib::platform::filesystem::GetDirectory(exePath) + "/shaders/bgfx/" + rendererDir;
    }
    if (baseDir.empty() || !eyelib::platform::filesystem::FileExists(baseDir + "/" + name + "_vs.bin"))
        baseDir = "shaders/bgfx/" + std::string(rendererDir);
    if (!eyelib::platform::filesystem::FileExists(baseDir + "/" + name + "_vs.bin"))
        baseDir = "assets/shaders/bgfx/" + std::string(rendererDir);

    std::string vsPath = baseDir + "/" + name + "_vs.bin";
    std::string fsPath = baseDir + "/" + name + "_fs.bin";
    if (!eyelib::platform::filesystem::FileExists(vsPath) || !eyelib::platform::filesystem::FileExists(fsPath))
        return false;

    std::vector<uint8_t> vsData = eyelib::platform::filesystem::ReadFile(vsPath);
    std::vector<uint8_t> fsData = eyelib::platform::filesystem::ReadFile(fsPath);
    if (vsData.empty() || fsData.empty()) return false;

    IShader* vs = CreateShaderFromBinary(ShaderStage::Vertex, vsData.data(), vsData.size());
    IShader* fs = CreateShaderFromBinary(ShaderStage::Fragment, fsData.data(), fsData.size());
    if (!vs || !fs) {
        if (vs) DestroyShader(vs);
        if (fs) DestroyShader(fs);
        return false;
    }
    outVs = vs;
    outFs = fs;
    return true;
}

IPipeline* DeviceBGFX::CreateGraphicsPipeline(const PipelineDesc& desc) {
    auto* p = new PipelineBGFX();
    p->m_vertexLayout = desc.vertexLayout;
    p->m_topology = desc.topology;
    p->m_blend = desc.blend;
    p->m_depthStencil = desc.depthStencil;
    p->m_raster = desc.raster;

    ShaderBGFX* vsBGFX = desc.vertexShader ? static_cast<ShaderBGFX*>(desc.vertexShader) : nullptr;
    ShaderBGFX* fsBGFX = desc.fragmentShader ? static_cast<ShaderBGFX*>(desc.fragmentShader) : nullptr;
    if (vsBGFX && fsBGFX && vsBGFX->m_shaderIdx != ShaderBGFX::kInvalidIdx && fsBGFX->m_shaderIdx != ShaderBGFX::kInvalidIdx) {
        bgfx::ShaderHandle vsH; vsH.idx = vsBGFX->m_shaderIdx;
        bgfx::ShaderHandle fsH; fsH.idx = fsBGFX->m_shaderIdx;
        bgfx::ProgramHandle prog = bgfx::createProgram(vsH, fsH, false);
        if (bgfx::isValid(prog))
            p->m_programIdx = prog.idx;
    }
    return p;
}

#if defined(_MSC_VER) && defined(_WIN32)
/** Helper with no C++ object unwinding so __try/__except is allowed. Catches FS create crash and returns nullptr. */
static IShader* CreateFragmentShaderFromBinaryNoThrow(DeviceBGFX* device, const void* data, size_t size) {
    IShader* fs = nullptr;
    __try {
        fs = device->CreateShaderFromBinary(ShaderStage::Fragment, data, size);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return fs;
}
#endif

IPipeline* DeviceBGFX::CreateGraphicsPipelineFromName(const char* shaderName, const VertexLayout& layout, PrimitiveTopology topology) {
    if (!shaderName || !shaderName[0]) return nullptr;
    core::Log::InfoF("DeviceBGFX::CreateGraphicsPipelineFromName: shaderName='%s'", shaderName);
    std::string name(shaderName);
    size_t lastSlash = name.rfind('/');
    std::string baseName = (lastSlash != std::string::npos) ? name.substr(lastSlash + 1) : name;
    if (baseName.empty()) return nullptr;

    const char* rendererDir = "gl";
    switch (bgfx::getRendererType()) {
        case bgfx::RendererType::OpenGL:
        case bgfx::RendererType::OpenGLES: rendererDir = "gl"; break;
        case bgfx::RendererType::Direct3D11: rendererDir = "d3d11"; break;
        case bgfx::RendererType::Vulkan: rendererDir = "vulkan"; break;
        default: rendererDir = "gl"; break;
    }
    core::Log::InfoF("DeviceBGFX::CreateGraphicsPipelineFromName: baseName='%s' rendererDir='%s'", baseName.c_str(), rendererDir);

    std::string baseDir;
    std::string exePath = eyelib::platform::filesystem::GetExecutablePath();
    if (!exePath.empty())
        baseDir = eyelib::platform::filesystem::GetDirectory(exePath) + "/assets/shaders/bgfx/" + rendererDir;
    if (baseDir.empty() || !eyelib::platform::filesystem::FileExists(baseDir + "/" + baseName + "_vs.bin"))
        baseDir = "assets/shaders/bgfx/" + std::string(rendererDir);

    std::string vsPath = baseDir + "/" + baseName + "_vs.bin";
    std::string fsPath = baseDir + "/" + baseName + "_fs.bin";
    core::Log::InfoF("DeviceBGFX::CreateGraphicsPipelineFromName: vsPath='%s' fsPath='%s'", vsPath.c_str(), fsPath.c_str());
    if (!eyelib::platform::filesystem::FileExists(vsPath) || !eyelib::platform::filesystem::FileExists(fsPath)) {
        core::Log::WarnF("DeviceBGFX::CreateGraphicsPipelineFromName: shader files not found");
        return nullptr;
    }

    core::Log::Info("DeviceBGFX::CreateGraphicsPipelineFromName: before ReadFile");
    std::vector<uint8_t> vsData = eyelib::platform::filesystem::ReadFile(vsPath);
    std::vector<uint8_t> fsData = eyelib::platform::filesystem::ReadFile(fsPath);
    if (vsData.empty() || fsData.empty()) {
        core::Log::WarnF("DeviceBGFX::CreateGraphicsPipelineFromName: ReadFile empty (vs=%zu fs=%zu)", vsData.size(), fsData.size());
        return nullptr;
    }
    core::Log::InfoF("DeviceBGFX::CreateGraphicsPipelineFromName: after ReadFile (vs=%zu fs=%zu bytes)", vsData.size(), fsData.size());

    core::Log::Info("DeviceBGFX::CreateGraphicsPipelineFromName: before CreateShaderFromBinary (VS)");
    IShader* vs = CreateShaderFromBinary(ShaderStage::Vertex, vsData.data(), vsData.size());
    if (!vs) return nullptr;
    core::Log::Info("DeviceBGFX::CreateGraphicsPipelineFromName: after VS, before CreateShaderFromBinary (FS)");
    IShader* fs = nullptr;
#if defined(_MSC_VER) && defined(_WIN32)
    fs = CreateFragmentShaderFromBinaryNoThrow(this, fsData.data(), fsData.size());
    if (!fs) {
        core::Log::Warn("DeviceBGFX::CreateGraphicsPipelineFromName: CreateShaderFromBinary(FS) crashed or failed, skipping pipeline");
        DestroyShader(vs);
        return nullptr;
    }
#else
    fs = CreateShaderFromBinary(ShaderStage::Fragment, fsData.data(), fsData.size());
#endif
    core::Log::Info("DeviceBGFX::CreateGraphicsPipelineFromName: after FS CreateShaderFromBinary");
    if (!fs) {
        DestroyShader(vs);
        return nullptr;
    }

    core::Log::Info("DeviceBGFX::CreateGraphicsPipelineFromName: before CreateGraphicsPipeline");
    PipelineDesc desc;
    desc.vertexShader = vs;
    desc.fragmentShader = fs;
    desc.vertexLayout = layout;
    desc.topology = topology;
    desc.depthStencil.depthTest = true;
    desc.depthStencil.depthWrite = true;
    IPipeline* pipe = CreateGraphicsPipeline(desc);
    core::Log::Info("DeviceBGFX::CreateGraphicsPipelineFromName: after CreateGraphicsPipeline");
    DestroyShader(vs);
    DestroyShader(fs);
    return pipe;
}

IPipeline* DeviceBGFX::CreateComputePipeline(void* computeShader) {
    (void)computeShader;
    return new PipelineBGFX();
}

void DeviceBGFX::DestroyPipeline(IPipeline* pipeline) {
    auto* p = static_cast<PipelineBGFX*>(pipeline);
    if (p && p->m_programIdx != PipelineBGFX::kInvalidIdx) {
        bgfx::ProgramHandle h; h.idx = p->m_programIdx;
        if (bgfx::isValid(h)) bgfx::destroy(h);
    }
    delete p;
}

uint16_t DeviceBGFX::GetOrCreateUniform(const char* name, UniformKind kind) {
    if (!name || !name[0]) return kInvalidIdx;
    // BGFX can crash when createUniform is called with names containing '[' (e.g. "u_PointLightPos[0]").
    if (std::strchr(name, '[')) return kInvalidIdx;
    auto it = m_uniformHandles.find(name);
    if (it != m_uniformHandles.end()) return it->second;
    bgfx::UniformType::Enum ut = bgfx::UniformType::Vec4;
    switch (kind) {
        case UniformKind::Vec4: ut = bgfx::UniformType::Vec4; break;
        case UniformKind::Mat4: ut = bgfx::UniformType::Mat4; break;
        case UniformKind::Sampler: ut = bgfx::UniformType::Sampler; break;
    }
    bgfx::UniformHandle h = bgfx::createUniform(name, ut);
    if (!bgfx::isValid(h)) return kInvalidIdx;
    uint16_t idx = h.idx;
    m_uniformHandles[name] = idx;
    return idx;
}

IRenderTarget* DeviceBGFX::CreateRenderTarget(uint32_t width, uint32_t height,
                                              const std::vector<RHITextureFormat>& colorFormats,
                                              RHITextureFormat depthFormat,
                                              uint32_t samples) {
    (void)samples;
    (void)depthFormat;
    auto* rt = new RenderTargetBGFX();
    rt->m_width = width;
    rt->m_height = height;

    RHITextureFormat rhiFmt = colorFormats.empty() ? RHITextureFormat::RGBA8 : colorFormats[0];
    bgfx::TextureFormat::Enum colorFmt = colorFormats.empty()
        ? bgfx::TextureFormat::RGBA8
        : static_cast<bgfx::TextureFormat::Enum>(BGFXEnums::TextureFormatToBGFX(rhiFmt));
    bgfx::FrameBufferHandle fb = bgfx::createFrameBuffer(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        colorFmt,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );
    rt->m_fbIdx = fb.idx;

    bgfx::TextureHandle colorTex = bgfx::getTexture(fb, 0);
    if (bgfx::isValid(colorTex)) {
        auto tex = std::make_unique<TextureBGFX>();
        tex->m_texIdx = colorTex.idx;
        tex->m_width = width;
        tex->m_height = height;
        tex->m_format = rhiFmt;
        rt->m_colorTextures.push_back(std::move(tex));
    }

    return rt;
}

IRenderTarget* DeviceBGFX::CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) {
    (void)face;
    (void)mipLevel;
    auto* rt = new RenderTargetBGFX();
    auto* tex = static_cast<TextureBGFX*>(colorTexture);
    if (!tex || tex->m_texIdx == TextureBGFX::kInvalidIdx) { delete rt; return nullptr; }
    rt->m_width = tex->m_width;
    rt->m_height = tex->m_height;
    bgfx::TextureHandle th; th.idx = tex->m_texIdx;
    rt->m_fbIdx = bgfx::createFrameBuffer(1, &th, false).idx;
    rt->m_externalColorTexture = colorTexture;
    return rt;
}

void DeviceBGFX::DestroyRenderTarget(IRenderTarget* rt) {
    auto* r = static_cast<RenderTargetBGFX*>(rt);
    if (r && r->m_fbIdx != RenderTargetBGFX::kInvalidIdx) {
        bgfx::FrameBufferHandle h; h.idx = r->m_fbIdx;
        if (bgfx::isValid(h)) bgfx::destroy(h);
    }
    delete r;
}

ITexture* DeviceBGFX::CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) {
    (void)nativeTextureId;
    (void)width;
    (void)height;
    (void)format;
    return nullptr;
}

IQueryPool* DeviceBGFX::CreateQueryPool(QueryType type, uint32_t count) {
    auto* q = new QueryPoolBGFX();
    q->m_type = type;
    q->m_count = count;
    return q;
}

void DeviceBGFX::DestroyQueryPool(IQueryPool* pool) {
    delete static_cast<QueryPoolBGFX*>(pool);
}

int DeviceBGFX::GetMaxSamples(int requestedSamples) {
    (void)requestedSamples;
    return 1;
}

void DeviceBGFX::Present() {
    bgfx::frame();
}

void DeviceBGFX::BlitToDefaultFramebuffer(IRenderTarget* src) {
    if (!src) return;
    EnsureBlitProgram();
    if (HasValidBlitProgram())
        BlitToBackbufferWithProgram(src);
}

void DeviceBGFX::BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) {
    (void)nativeFbo;
    (void)width;
    (void)height;
}

void DeviceBGFX::Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) {
    (void)src;
    (void)dst;
    (void)colorOnly;
}

void DeviceBGFX::BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) {
    (void)nativeFbo;
    (void)dst;
    (void)colorOnly;
}

void DeviceBGFX::BlitDepthOnly(IRenderTarget* src, IRenderTarget* dst) {
    (void)src;
    (void)dst;
}

void DeviceBGFX::EnsureBlitProgram() {
    if (m_blitProgramTried) return;
    m_blitProgramTried = true;

    const char* rendererDir = "gl";
    switch (bgfx::getRendererType()) {
        case bgfx::RendererType::OpenGL:
        case bgfx::RendererType::OpenGLES: rendererDir = "gl"; break;
        case bgfx::RendererType::Direct3D11: rendererDir = "d3d11"; break;
        case bgfx::RendererType::Vulkan: rendererDir = "vulkan"; break;
        default: rendererDir = "gl"; break;
    }

    std::string baseDir;
    std::string exePath = eyelib::platform::filesystem::GetExecutablePath();
    if (!exePath.empty()) {
        baseDir = eyelib::platform::filesystem::GetDirectory(exePath) + "/shaders/bgfx/" + rendererDir;
    }
    if (baseDir.empty() || !eyelib::platform::filesystem::FileExists(baseDir + "/copy_vs.bin"))
        baseDir = "shaders/bgfx/" + std::string(rendererDir);
    if (!eyelib::platform::filesystem::FileExists(baseDir + "/copy_vs.bin"))
        baseDir = "assets/shaders/bgfx/" + std::string(rendererDir);

    std::vector<uint8_t> vsData = eyelib::platform::filesystem::ReadFile(baseDir + "/copy_vs.bin");
    std::vector<uint8_t> fsData = eyelib::platform::filesystem::ReadFile(baseDir + "/copy_fs.bin");
    if (vsData.empty() || fsData.empty()) return;

    IShader* vs = CreateShaderFromBinary(ShaderStage::Vertex, vsData.data(), vsData.size());
    IShader* fs = CreateShaderFromBinary(ShaderStage::Fragment, fsData.data(), fsData.size());
    if (!vs || !fs) {
        if (vs) DestroyShader(vs);
        if (fs) DestroyShader(fs);
        return;
    }

    ShaderBGFX* vsBGFX = static_cast<ShaderBGFX*>(vs);
    ShaderBGFX* fsBGFX = static_cast<ShaderBGFX*>(fs);
    bgfx::ShaderHandle vsH; vsH.idx = vsBGFX->m_shaderIdx;
    bgfx::ShaderHandle fsH; fsH.idx = fsBGFX->m_shaderIdx;
    bgfx::ProgramHandle prog = bgfx::createProgram(vsH, fsH, true);
    DestroyShader(vs);
    DestroyShader(fs);
    if (!bgfx::isValid(prog)) return;
    m_blitProgramIdx = prog.idx;

    bgfx::UniformHandle texUniform = bgfx::createUniform("u_Texture", bgfx::UniformType::Sampler);
    if (bgfx::isValid(texUniform)) m_blitTextureUniformIdx = texUniform.idx;

    struct PosTexVertex { float x, y, u, v; };
    const PosTexVertex quadVerts[] = {
        {-1.f, -1.f, 0.f, 1.f},
        { 1.f, -1.f, 1.f, 1.f},
        {-1.f,  1.f, 0.f, 0.f},
        { 1.f,  1.f, 1.f, 0.f},
    };
    const uint16_t quadIndices[] = { 0, 1, 2, 1, 3, 2 };

    bgfx::VertexLayout layout;
    layout.begin().add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float).add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float).end();

    const bgfx::Memory* vbMem = bgfx::copy(quadVerts, sizeof(quadVerts));
    bgfx::VertexBufferHandle vb = bgfx::createVertexBuffer(vbMem, layout);
    if (bgfx::isValid(vb)) m_fullscreenVbIdx = vb.idx;

    const bgfx::Memory* ibMem = bgfx::copy(quadIndices, sizeof(quadIndices));
    bgfx::IndexBufferHandle ib = bgfx::createIndexBuffer(ibMem);
    if (bgfx::isValid(ib)) m_fullscreenIbIdx = ib.idx;
}

bool DeviceBGFX::HasValidBlitProgram() const {
    if (m_blitProgramIdx == kInvalidIdx || m_fullscreenVbIdx == kInvalidIdx || m_fullscreenIbIdx == kInvalidIdx) return false;
    bgfx::ProgramHandle h; h.idx = m_blitProgramIdx;
    return bgfx::isValid(h);
}

void DeviceBGFX::BlitToBackbufferWithProgram(IRenderTarget* src) {
    ITexture* colorTex = src->GetColorTexture(0);
    if (!colorTex) return;
    TextureBGFX* tex = static_cast<TextureBGFX*>(colorTex);
    if (tex->m_texIdx == TextureBGFX::kInvalidIdx) return;

    const uint16_t viewId = 0;
    bgfx::setViewFrameBuffer(viewId, BGFX_INVALID_HANDLE);
    bgfx::setViewRect(viewId, 0, 0, static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height));
    bgfx::setViewClear(viewId, BGFX_CLEAR_NONE, 0, 1.f, 0);

    bgfx::ProgramHandle prog; prog.idx = m_blitProgramIdx;
    bgfx::VertexBufferHandle vb; vb.idx = m_fullscreenVbIdx;
    bgfx::IndexBufferHandle ib; ib.idx = m_fullscreenIbIdx;
    bgfx::TextureHandle th; th.idx = tex->m_texIdx;

    bgfx::setVertexBuffer(0, vb);
    bgfx::setIndexBuffer(ib);
    if (m_blitTextureUniformIdx != kInvalidIdx) {
        bgfx::UniformHandle uh; uh.idx = m_blitTextureUniformIdx;
        bgfx::setTexture(0, uh, th);
    }
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::submit(viewId, prog);
}

} // namespace rhi
} // namespace render
} // namespace eyelib
