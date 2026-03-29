#include "eyelib/render/rhi/diligent/DeviceDiligent.h"
#include "eyelib/render/rhi/diligent/BufferDiligent.h"
#include "eyelib/render/rhi/diligent/TextureDiligent.h"
#include "eyelib/render/rhi/diligent/ShaderDiligent.h"
#include "eyelib/render/rhi/diligent/PipelineDiligent.h"
#include "eyelib/render/rhi/diligent/PipelineLayoutDiligent.h"
#include "eyelib/render/rhi/diligent/DescriptorSetDiligent.h"
#include "eyelib/render/rhi/diligent/RenderTargetDiligent.h"
#include "eyelib/render/rhi/diligent/QueryPoolDiligent.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/render/rhi/PipelineAttachmentFormatSets.h"
#include "eyelib/render/ShaderPathResolve.h"
#include "eyelib/render/backend/RenderBackendPolicy.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#if OPENEYE_USE_DILIGENT
#include "eyelib/render/rhi/diligent/DeviceDiligentImpl.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/InputLayout.h"
#include "Graphics/GraphicsEngine/interface/PipelineResourceSignature.h"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/Sampler.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Common/interface/RefCntAutoPtr.hpp"
#endif

namespace eyelib {
namespace render {
namespace rhi {

namespace {
#if OPENEYE_USE_DILIGENT
static Diligent::TEXTURE_FORMAT RHIToDiligentTextureFormat(RHITextureFormat fmt) {
    using namespace Diligent;
    switch (fmt) {
        case RHITextureFormat::R8: return TEX_FORMAT_R8_UNORM;
        case RHITextureFormat::RG8: return TEX_FORMAT_RG8_UNORM;
        case RHITextureFormat::RGB8: return TEX_FORMAT_RGBA8_UNORM;
        case RHITextureFormat::RGBA8: return TEX_FORMAT_RGBA8_UNORM;
        case RHITextureFormat::RGB8_SRGB: return TEX_FORMAT_RGB8_UNORM_SRGB;
        case RHITextureFormat::RGBA8_SRGB: return TEX_FORMAT_RGBA8_UNORM_SRGB;
        case RHITextureFormat::BGRA8: return TEX_FORMAT_BGRA8_UNORM;
        case RHITextureFormat::RGB16F: return TEX_FORMAT_RGBA16_FLOAT;
        case RHITextureFormat::RGBA16F: return TEX_FORMAT_RGBA16_FLOAT;
        case RHITextureFormat::R32UI: return TEX_FORMAT_R32_UINT;
        case RHITextureFormat::Depth24: return TEX_FORMAT_D32_FLOAT;
        case RHITextureFormat::Depth24Stencil8: return TEX_FORMAT_D24_UNORM_S8_UINT;
        default: return TEX_FORMAT_RGBA8_UNORM;
    }
}
#endif
static std::string ApplyKeywordDefinesDiligent(const std::string& source, const std::vector<std::string>& keywords) {
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
} // namespace

DeviceDiligent::DeviceDiligent(void* nwh, uint32_t width, uint32_t height, DeviceDiligentImpl* impl)
    : m_impl(impl), m_width(width), m_height(height) {
    (void)nwh;
    m_caps.supportsCompute = true;   // Diligent supports compute
    m_caps.supportsMultiDrawIndirect = true;  // D3D12/Vk support it; D3D11/GL depend on impl
    m_immediateCommandList = std::make_unique<CommandListDiligent>(this);
}

DeviceDiligent::~DeviceDiligent() {
#if OPENEYE_USE_DILIGENT
    if (m_blitPipeline) { DestroyPipeline(m_blitPipeline); m_blitPipeline = nullptr; }
    if (m_blitVB) { DestroyBuffer(m_blitVB); m_blitVB = nullptr; }
    if (m_blitIB) { DestroyBuffer(m_blitIB); m_blitIB = nullptr; }
    if (m_blitSampler) {
        static_cast<Diligent::ISampler*>(m_blitSampler)->Release();
        m_blitSampler = nullptr;
    }
#endif
    m_immediateCommandList.reset();
    m_impl.reset();
}

const RHICapabilities& DeviceDiligent::GetCapabilities() const {
    return m_caps;
}

const char* DeviceDiligent::GetShaderBackendName() const {
    return "diligent";
}

const char* DeviceDiligent::GetShaderVertexExtension() const {
    return ".vsh";
}

const char* DeviceDiligent::GetShaderFragmentExtension() const {
    return ".psh";
}

void* DeviceDiligent::GetImmediateContext() const {
#if OPENEYE_USE_DILIGENT
    if (m_impl && m_impl->pContext) return m_impl->pContext.RawPtr();
#endif
    return nullptr;
}

void* DeviceDiligent::GetRenderDevice() const {
#if OPENEYE_USE_DILIGENT
    if (m_impl && m_impl->pDevice) return m_impl->pDevice.RawPtr();
#endif
    return nullptr;
}

void* DeviceDiligent::GetSwapChainBackBufferRTV() const {
#if OPENEYE_USE_DILIGENT
    if (m_impl && m_impl->pSwapChain) return m_impl->pSwapChain->GetCurrentBackBufferRTV();
#endif
    return nullptr;
}

void* DeviceDiligent::GetLinearSampler() {
#if OPENEYE_USE_DILIGENT
    EnsureBlitResources();
    return m_blitSampler;
#else
    return nullptr;
#endif
}

ICommandList* DeviceDiligent::CreateCommandList() {
    return nullptr;  // Single immediate list for now
}

void DeviceDiligent::DestroyCommandList(ICommandList* list) {
    (void)list;
}

ICommandList* DeviceDiligent::GetImmediateCommandList() {
    return m_immediateCommandList.get();
}

void DeviceDiligent::SubmitCommandList(ICommandList* list) {
    (void)list;
}

IBuffer* DeviceDiligent::CreateBuffer(const BufferDesc& desc, const void* initialData) {
    auto* b = new BufferDiligent();
    b->m_size = desc.sizeInBytes;
#if OPENEYE_USE_DILIGENT
    if (m_impl && m_impl->pDevice && desc.sizeInBytes > 0) {
        const bool isVertex = HasUsage(desc.usage, BufferUsage::Vertex);
        const bool isIndex = HasUsage(desc.usage, BufferUsage::Index);
        if (isVertex || isIndex) {
            Diligent::BufferDesc d;
            d.Size = static_cast<Diligent::Uint64>(desc.sizeInBytes);
            d.BindFlags = isVertex ? Diligent::BIND_VERTEX_BUFFER : Diligent::BIND_INDEX_BUFFER;
            d.Usage = (initialData && desc.sizeInBytes > 0) ? Diligent::USAGE_IMMUTABLE : Diligent::USAGE_DEFAULT;
            d.Mode = Diligent::BUFFER_MODE_STRUCTURED;
            d.ElementByteStride = 0;
            Diligent::BufferData bdata(initialData, desc.sizeInBytes, m_impl->pContext);
            Diligent::IBuffer* pNative = nullptr;
            m_impl->pDevice->CreateBuffer(d, initialData ? &bdata : nullptr, &pNative);
            if (pNative) {
                pNative->AddRef();
                b->m_pNative = pNative;
                return b;
            }
        }
    }
#endif
    if (desc.memoryHint == BufferMemoryHint::HostVisible)
        b->m_cpuData.resize(desc.sizeInBytes);
    if (initialData && desc.sizeInBytes > 0) {
        if (!b->m_cpuData.empty())
            std::memcpy(b->m_cpuData.data(), initialData, desc.sizeInBytes);
    }
    return b;
}

void DeviceDiligent::DestroyBuffer(IBuffer* buffer) {
    if (buffer)
        delete static_cast<BufferDiligent*>(buffer);
}

ITexture* DeviceDiligent::CreateTexture(const TextureDesc& desc, const void* initialData) {
    (void)initialData;
    auto* t = new TextureDiligent();
    t->m_width = desc.width;
    t->m_height = desc.height;
    t->m_depthOrArray = desc.depthOrArrayLayers;
    t->m_type = desc.type;
    t->m_format = desc.format;
    return t;
}

void DeviceDiligent::DestroyTexture(ITexture* texture) {
    if (texture)
        delete static_cast<TextureDiligent*>(texture);
}

void DeviceDiligent::CommitSparseTexturePages(ITexture* texture, uint32_t level, int32_t x, int32_t y, int32_t z, uint32_t width, uint32_t height, uint32_t depth, bool commit) {
    (void)texture; (void)level; (void)x; (void)y; (void)z; (void)width; (void)height; (void)depth; (void)commit;
}

void DeviceDiligent::CommitSparseBufferPages(IBuffer* buffer, size_t offset, size_t size, bool commit) {
    (void)buffer; (void)offset; (void)size; (void)commit;
}

ITexture* DeviceDiligent::CreateTextureView(ITexture* source, RHITextureFormat viewFormat, uint32_t baseMipLevel, uint32_t mipLevels, uint32_t baseLayer, uint32_t layerCount) {
    (void)source; (void)viewFormat; (void)baseMipLevel; (void)mipLevels; (void)baseLayer; (void)layerCount;
    return nullptr;
}

ITexture* DeviceDiligent::CreateTextureBuffer(IBuffer* buffer, RHITextureFormat format, size_t offset, size_t size) {
    (void)buffer; (void)format; (void)offset; (void)size;
    return nullptr;
}

bool DeviceDiligent::WaitFence(IFence* fence, uint64_t timeoutMs) {
    (void)fence; (void)timeoutMs;
    return true;
}

void DeviceDiligent::DestroyFence(IFence* fence) {
    (void)fence;
}

IShader* DeviceDiligent::CreateShader(ShaderStage stage, const char* source, size_t sourceLen) {
#if OPENEYE_USE_DILIGENT
    // Diligent expects HLSL; reject GLSL (#version) to avoid Vulkan/GL compiler errors
    if (source && sourceLen >= 8 && std::strstr(source, "#version") != nullptr) {
        auto* s = new ShaderDiligent();
        s->m_stage = stage;
        s->m_valid = false;
        return s;
    }
    if (m_impl && m_impl->pDevice && source && sourceLen > 0) {
        using namespace Diligent;
        ShaderCreateInfo ci;
        ci.Source       = source;
        ci.SourceLength = sourceLen;
        ci.EntryPoint   = "main";
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = (stage == ShaderStage::Vertex) ? SHADER_TYPE_VERTEX : SHADER_TYPE_PIXEL;
        Diligent::IShader* pDgShader = nullptr;
        m_impl->pDevice->CreateShader(ci, &pDgShader, nullptr);
        if (pDgShader) {
            auto* s = new ShaderDiligent();
            s->m_stage = stage;
            s->m_valid = true;
            s->m_pNative = pDgShader;
            return s;
        }
    }
#endif
    auto* s = new ShaderDiligent();
    s->m_stage = stage;
    s->m_valid = false;
    return s;
}

IShader* DeviceDiligent::CreateShaderFromBinary(ShaderStage stage, const void* data, size_t size) {
    (void)data;
    (void)size;
    auto* s = new ShaderDiligent();
    s->m_stage = stage;
    s->m_valid = false;
    return s;
}

bool DeviceDiligent::TryLoadShaderFromPaths(const char* vertexPath, const char* fragmentPath, IShader*& outVs, IShader*& outFs) {
    outVs = nullptr;
    outFs = nullptr;
#if OPENEYE_USE_DILIGENT
    if (!m_impl || !m_impl->pDevice || !vertexPath || !fragmentPath) return false;
    using namespace eyelib::platform::filesystem;
    std::string vert(vertexPath);
    std::string frag(fragmentPath);
    vert = NormalizePath(vert);
    frag = NormalizePath(frag);
#ifdef _WIN32
    for (char& c : vert) if (c == '\\') c = '/';
    for (char& c : frag) if (c == '\\') c = '/';
#endif
    // Logical path is passed (e.g. passes/forward/skinned.vert). Strip extension, then prepend assets/shaders/diligent/.
    std::string base = vert;
    if (base.size() >= 5 && base.compare(base.size() - 5, 5, ".vert") == 0)
        base.resize(base.size() - 5);
    else if (base.size() >= 5 && base.compare(base.size() - 5, 5, ".frag") == 0)
        base.resize(base.size() - 5);
    else
        return false;
    if (base.size() > 14 && base.compare(0, 14, "assets/shaders/") == 0)
        base = base.substr(14);
    if (base.size() > 7 && base.compare(0, 7, "opengl/") == 0)
        base = base.substr(7);
    const std::string baseDir = "assets/shaders/diligent/";
    std::string vshPath, pshPath;
    std::string exeDir = GetDirectory(GetExecutablePath());
    if (!exeDir.empty() && FileExists(exeDir + baseDir + base + ".vsh")) {
        vshPath = exeDir + baseDir + base + ".vsh";
        pshPath = exeDir + baseDir + base + ".psh";
    } else {
        vshPath = baseDir + base + ".vsh";
        pshPath = baseDir + base + ".psh";
    }
    if (!FileExists(vshPath) || !FileExists(pshPath)) return false;
    std::vector<uint8_t> vsBuf = ReadFile(vshPath);
    std::vector<uint8_t> psBuf = ReadFile(pshPath);
    if (vsBuf.empty() || psBuf.empty()) return false;
    vsBuf.push_back(0);
    psBuf.push_back(0);
    const char* vsSrc = reinterpret_cast<const char*>(vsBuf.data());
    const char* psSrc = reinterpret_cast<const char*>(psBuf.data());
    outVs = CreateShader(ShaderStage::Vertex, vsSrc, vsBuf.size() - 1);
    outFs = CreateShader(ShaderStage::Fragment, psSrc, psBuf.size() - 1);
    if (!outVs || !static_cast<ShaderDiligent*>(outVs)->IsValid() ||
        !outFs || !static_cast<ShaderDiligent*>(outFs)->IsValid()) {
        if (outVs) { DestroyShader(outVs); outVs = nullptr; }
        if (outFs) { DestroyShader(outFs); outFs = nullptr; }
        return false;
    }
    return true;
#endif
    return false;
}

void DeviceDiligent::DestroyShader(IShader* shader) {
    if (shader)
        delete static_cast<ShaderDiligent*>(shader);
}

IPipeline* DeviceDiligent::CreateGraphicsPipeline(const PipelineDesc& desc) {
    auto* p = new PipelineDiligent();
    p->m_vertexLayout = desc.vertexLayout;
    p->m_topology = desc.topology;
    p->m_blend = desc.blend;
    p->m_depthStencil = desc.depthStencil;
    p->m_raster = desc.raster;

#if OPENEYE_USE_DILIGENT
    if (m_impl && m_impl->pDevice && desc.vertexShader && desc.fragmentShader) {
        ShaderDiligent* pVs = static_cast<ShaderDiligent*>(desc.vertexShader);
        ShaderDiligent* pFs = static_cast<ShaderDiligent*>(desc.fragmentShader);
        if (pVs->IsValid() && pFs->IsValid() && pVs->m_pNative && pFs->m_pNative) {
            using namespace Diligent;
            static constexpr Uint32 MAX_EL = 12u;
            LayoutElement layoutElements[MAX_EL];
            Uint32 numElements = 0;
            const VertexLayout& layout = desc.vertexLayout;
            if (!layout.attributes.empty() && layout.attributes.size() <= MAX_EL) {
                for (const auto& a : layout.attributes) {
                    LayoutElement& e = layoutElements[numElements++];
                    e.HLSLSemantic = "ATTRIB";
                    e.InputIndex = a.location;
                    e.BufferSlot = 0;
                    e.RelativeOffset = a.offsetInBytes;
                    e.Stride = layout.strideInBytes;
                    e.Frequency = INPUT_ELEMENT_FREQUENCY_PER_VERTEX;
                    switch (a.type) {
                        case VertexAttributeType::Float:  e.NumComponents = 1; e.ValueType = VT_FLOAT32; break;
                        case VertexAttributeType::Float2: e.NumComponents = 2; e.ValueType = VT_FLOAT32; break;
                        case VertexAttributeType::Float3: e.NumComponents = 3; e.ValueType = VT_FLOAT32; break;
                        case VertexAttributeType::Float4: e.NumComponents = 4; e.ValueType = VT_FLOAT32; break;
                        case VertexAttributeType::Int:     e.NumComponents = 1; e.ValueType = VT_INT32; break;
                        case VertexAttributeType::Int2:    e.NumComponents = 2; e.ValueType = VT_INT32; break;
                        case VertexAttributeType::Int3:   e.NumComponents = 3; e.ValueType = VT_INT32; break;
                        case VertexAttributeType::Int4:   e.NumComponents = 4; e.ValueType = VT_INT32; break;
                        case VertexAttributeType::UInt4:   e.NumComponents = 4; e.ValueType = VT_UINT32; break;
                        default: e.NumComponents = 3; e.ValueType = VT_FLOAT32; break;
                    }
                    e.IsNormalized = false;
                }
            }
            if (numElements == 0) { numElements = 2; layoutElements[0] = LayoutElement("ATTRIB", 0, 0, 2, VT_FLOAT32, false, 0, 16, INPUT_ELEMENT_FREQUENCY_PER_VERTEX); layoutElements[1] = LayoutElement("ATTRIB", 1, 0, 2, VT_FLOAT32, false, 8, 16, INPUT_ELEMENT_FREQUENCY_PER_VERTEX); }
            InputLayoutDesc inputLayoutDesc;
            inputLayoutDesc.LayoutElements = layoutElements;
            inputLayoutDesc.NumElements = numElements;

            GraphicsPipelineStateCreateInfo psoci;
            psoci.PSODesc.Name = "PipelineFromDesc";
            psoci.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
            psoci.pVS = static_cast<Diligent::IShader*>(pVs->m_pNative);
            psoci.pPS = static_cast<Diligent::IShader*>(pFs->m_pNative);
            psoci.GraphicsPipeline.InputLayout = inputLayoutDesc;
            psoci.GraphicsPipeline.PrimitiveTopology = (desc.topology == PrimitiveTopology::TriangleList) ? PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            psoci.GraphicsPipeline.NumRenderTargets = 1;
            psoci.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM_SRGB;
            if (m_impl->pSwapChain) psoci.GraphicsPipeline.RTVFormats[0] = m_impl->pSwapChain->GetDesc().ColorBufferFormat;
            psoci.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;

            Diligent::IPipelineState* pPSOPtr = nullptr;
            m_impl->pDevice->CreateGraphicsPipelineState(psoci, &pPSOPtr);
            if (pPSOPtr) {
                pPSOPtr->AddRef();
                p->m_pNative = pPSOPtr;
            }
        }
    }
#endif
    return p;
}

IPipeline* DeviceDiligent::CreateGraphicsPipelineFromShaderAsset(const ShaderGraphicsPipelineCreateInfo& info) {
    auto* p = new PipelineDiligent();
    p->m_vertexLayout = info.vertexLayout;
    p->m_topology = info.topology;

#if OPENEYE_USE_DILIGENT
    if (!m_impl || !m_impl->pDevice) {
        delete p;
        return nullptr;
    }

    const bool fromOverrides = !info.vertexSourceOverride.empty() && !info.fragmentSourceOverride.empty();
    if (!fromOverrides && info.assetPath.empty()) {
        delete p;
        return nullptr;
    }

    using namespace Diligent;
    using namespace eyelib::platform::filesystem;

    std::vector<uint8_t> vsBuf;
    std::vector<uint8_t> psBuf;
    if (fromOverrides) {
        const std::string vsStr = ApplyKeywordDefinesDiligent(info.vertexSourceOverride, info.keywordDefines);
        const std::string psStr = ApplyKeywordDefinesDiligent(info.fragmentSourceOverride, info.keywordDefines);
        vsBuf.assign(vsStr.begin(), vsStr.end());
        psBuf.assign(psStr.begin(), psStr.end());
        vsBuf.push_back(0);
        psBuf.push_back(0);
    } else {
        const std::string logicalName = eyelib::render::ShaderPathToLogicalName(info.assetPath);
        if (logicalName.empty()) {
            delete p;
            return nullptr;
        }
        std::string baseDir = "assets/shaders/diligent/";
        std::string vshPath, pshPath;
        std::string exeDir = GetDirectory(GetExecutablePath());
        if (!exeDir.empty() && FileExists(exeDir + baseDir + logicalName + ".vsh")) {
            vshPath = exeDir + baseDir + logicalName + ".vsh";
            pshPath = exeDir + baseDir + logicalName + ".psh";
        } else {
            vshPath = baseDir + logicalName + ".vsh";
            pshPath = baseDir + logicalName + ".psh";
        }
        if (!FileExists(vshPath) || !FileExists(pshPath)) {
            delete p;
            return nullptr;
        }
        vsBuf = ReadFile(vshPath);
        psBuf = ReadFile(pshPath);
        if (vsBuf.empty() || psBuf.empty()) {
            delete p;
            return nullptr;
        }
        vsBuf.push_back(0);
        psBuf.push_back(0);
    }
    const char* vsSrc = reinterpret_cast<const char*>(vsBuf.data());
    const char* psSrc = reinterpret_cast<const char*>(psBuf.data());

    IShader* pRhiVs = CreateShader(ShaderStage::Vertex, vsSrc, vsBuf.size() - 1);
    IShader* pRhiPs = CreateShader(ShaderStage::Fragment, psSrc, psBuf.size() - 1);
    ShaderDiligent* pVs = static_cast<ShaderDiligent*>(pRhiVs);
    ShaderDiligent* pPs = static_cast<ShaderDiligent*>(pRhiPs);
    if (!pVs->IsValid() || !pPs->IsValid()) {
        DestroyShader(pRhiVs);
        DestroyShader(pRhiPs);
        delete p;
        return nullptr;
    }

    Diligent::IShader* pDgVs = static_cast<Diligent::IShader*>(pVs->m_pNative);
    Diligent::IShader* pDgPs = static_cast<Diligent::IShader*>(pPs->m_pNative);

    // Input layout from VertexLayout or default fullscreen quad
    static constexpr Uint32 MAX_EL = 8u;
    LayoutElement layoutElements[MAX_EL];
    Uint32 numElements = 0;
    if (!info.vertexLayout.attributes.empty() && info.vertexLayout.attributes.size() <= MAX_EL) {
        for (const auto& a : info.vertexLayout.attributes) {
            LayoutElement& e = layoutElements[numElements++];
            e.HLSLSemantic = "ATTRIB";
            e.InputIndex = a.location;
            e.BufferSlot = 0;
            e.RelativeOffset = a.offsetInBytes;
            e.Stride = info.vertexLayout.strideInBytes;
            e.Frequency = INPUT_ELEMENT_FREQUENCY_PER_VERTEX;
            switch (a.type) {
                case VertexAttributeType::Float:  e.NumComponents = 1; e.ValueType = VT_FLOAT32; break;
                case VertexAttributeType::Float2:   e.NumComponents = 2; e.ValueType = VT_FLOAT32; break;
                case VertexAttributeType::Float3:   e.NumComponents = 3; e.ValueType = VT_FLOAT32; break;
                case VertexAttributeType::Float4:   e.NumComponents = 4; e.ValueType = VT_FLOAT32; break;
                case VertexAttributeType::Int:      e.NumComponents = 1; e.ValueType = VT_INT32; break;
                case VertexAttributeType::Int2:     e.NumComponents = 2; e.ValueType = VT_INT32; break;
                case VertexAttributeType::Int3:     e.NumComponents = 3; e.ValueType = VT_INT32; break;
                case VertexAttributeType::Int4:     e.NumComponents = 4; e.ValueType = VT_INT32; break;
                case VertexAttributeType::UInt4:    e.NumComponents = 4; e.ValueType = VT_UINT32; break;
                default: e.NumComponents = 3; e.ValueType = VT_FLOAT32; break;
            }
            e.IsNormalized = false;
        }
    } else {
        // Default fullscreen quad: position float2, texcoord float2, stride 16
        layoutElements[0] = LayoutElement("ATTRIB", 0, 0, 2, VT_FLOAT32, false, 0, 16, INPUT_ELEMENT_FREQUENCY_PER_VERTEX);
        layoutElements[1] = LayoutElement("ATTRIB", 1, 0, 2, VT_FLOAT32, false, 8, 16, INPUT_ELEMENT_FREQUENCY_PER_VERTEX);
        numElements = 2;
    }
    InputLayoutDesc inputLayoutDesc;
    inputLayoutDesc.LayoutElements = layoutElements;
    inputLayoutDesc.NumElements = numElements;

    // Pipeline Resource Signature: declare every cbuffer/texture/sampler the shaders use.
    // Names must match HLSL exactly (e.g. "Constants", "Material", "Bones", "u_ShadowMap").
    const std::string nameStr = fromOverrides ? (info.assetPath.empty() ? std::string("(inline)") : info.assetPath) : info.assetPath;
    const bool isTest = (nameStr.find("test/") == 0);
    const bool isForward = (nameStr.find("passes/forward/") != std::string::npos);
    const bool isShadow = (nameStr.find("passes/shadow/") != std::string::npos);
    const bool isPost = (nameStr.find("passes/post/") != std::string::npos);
    const bool isSkinned = (nameStr.find("skinned") != std::string::npos);
    std::vector<PipelineResourceDesc> resourceList;
    if (isForward) {
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_VERTEX, "Constants", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        if (isSkinned)
            resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_VERTEX, "Bones", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "Constants", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "Material", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "u_ShadowMap", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
    } else if (isShadow) {
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_VERTEX, "Constants", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        if (isSkinned)
            resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_VERTEX, "Bones", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
    } else if (isPost) {
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "Constants", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "u_HDRTexture", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "u_Texture", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "u_SceneTexture", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "u_BloomTexture", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "g_Position", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "g_Normal", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "g_Depth", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "g_Albedo", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "g_Velocity", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "u_Skybox", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "u_LinearSampler", 1u, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "u_Texture_sampler", 1u, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
    } else if (nameStr == "test/cube") {
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_VERTEX, "Constants", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
    } else {
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "u_Texture", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "u_Texture_sampler", 1u, SHADER_RESOURCE_TYPE_SAMPLER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
    }
    PipelineResourceSignatureDesc prsDesc;
    prsDesc.Name = isTest ? "TestPRS" : (isForward ? "ForwardPRS" : (isShadow ? "ShadowPRS" : (isPost ? "PostPRS" : "CopyPRS")));
    prsDesc.Resources = resourceList.data();
    prsDesc.NumResources = static_cast<Uint32>(resourceList.size());
    prsDesc.UseCombinedTextureSamplers = isForward;
    prsDesc.CombinedSamplerSuffix = "_sampler";
    Diligent::IPipelineResourceSignature* pPRSPtr = nullptr;
    m_impl->pDevice->CreatePipelineResourceSignature(prsDesc, &pPRSPtr);
    RefCntAutoPtr<Diligent::IPipelineResourceSignature> pPRS;
    if (pPRSPtr) pPRS.Attach(pPRSPtr);
    if (!pPRS)
        return p;

    GraphicsPipelineStateCreateInfo psoci;
    psoci.PSODesc.Name = nameStr.c_str();
    psoci.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    psoci.pVS = pDgVs;
    psoci.pPS = pDgPs;
    psoci.GraphicsPipeline.InputLayout = inputLayoutDesc;
    psoci.GraphicsPipeline.PrimitiveTopology = (info.topology == PrimitiveTopology::TriangleList)
        ? PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
        : (info.topology == PrimitiveTopology::TriangleStrip)
            ? PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
            : (info.topology == PrimitiveTopology::PointList)
                ? PRIMITIVE_TOPOLOGY_POINT_LIST
                : PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    if (info.attachmentFormats.has_value()) {
        const PipelineAttachmentFormats& af = *info.attachmentFormats;
        static constexpr Uint32 kMaxMrt = 8u;
        const size_t nColors = af.color.size();
        psoci.GraphicsPipeline.NumRenderTargets = static_cast<Uint32>(std::min(nColors, size_t(kMaxMrt)));
        for (Uint32 i = 0; i < psoci.GraphicsPipeline.NumRenderTargets; ++i)
            psoci.GraphicsPipeline.RTVFormats[i] = RHIToDiligentTextureFormat(af.color[i]);
        if (af.depthStencil == RHITextureFormat::None) {
            psoci.GraphicsPipeline.DSVFormat = TEX_FORMAT_UNKNOWN;
            psoci.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;
            psoci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
        } else {
            psoci.GraphicsPipeline.DSVFormat = RHIToDiligentTextureFormat(af.depthStencil);
            psoci.GraphicsPipeline.DepthStencilDesc.DepthEnable = true;
            psoci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = true;
        }
        if (info.layoutKind != PipelineLayoutKind::Shadow)
            psoci.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
    } else if (isShadow) {
        psoci.GraphicsPipeline.NumRenderTargets = 0;
        psoci.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
    } else {
        psoci.GraphicsPipeline.NumRenderTargets = 1;
        if (isForward) {
            psoci.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA16_FLOAT;
            psoci.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
            psoci.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;  // match SetCullFace(false)
        } else {
            // No depth buffer for post/test/copy; disable depth so draw succeeds
            psoci.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;
            psoci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;
            // Fullscreen quad is visible from +Z; disable cull for post/test/copy.
            // test/cube: no cull so cube is visible (no depth buffer; winding can hide faces otherwise).
            psoci.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
            if (isPost) {
                // Bloom stages render to HDR RTs (RGBA16F); tonemap/composite/fxaa/copy render to LDR (RGBA8) or swapchain.
                if (nameStr == "passes/post/bloom_extract" || nameStr == "passes/post/bloom_blur")
                    psoci.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA16_FLOAT;
                else if (nameStr == "passes/post/copy" && m_impl->pSwapChain)
                    psoci.GraphicsPipeline.RTVFormats[0] = m_impl->pSwapChain->GetDesc().ColorBufferFormat;
                else
                    psoci.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
            } else {
                // test/*: match swap chain when drawing to screen, else RGBA8_UNORM for offscreen RT
                if (isTest) {
                    if (m_impl->pSwapChain) {
                        const auto& scDesc = m_impl->pSwapChain->GetDesc();
                        psoci.GraphicsPipeline.RTVFormats[0] = scDesc.ColorBufferFormat;
                    } else
                        psoci.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
                } else {
                    psoci.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM_SRGB;
                    if (m_impl->pSwapChain) {
                        const auto& scDesc = m_impl->pSwapChain->GetDesc();
                        psoci.GraphicsPipeline.RTVFormats[0] = scDesc.ColorBufferFormat;
                    }
                }
            }
        }
    }
    // test/cube: explicit rasterizer. CW = front (D3D default). Cube indices wound CW from outside.
    if (nameStr == "test/cube") {
        auto& r = psoci.GraphicsPipeline.RasterizerDesc;
        r.FillMode = Diligent::FILL_MODE_SOLID;
        r.CullMode = Diligent::CULL_MODE_BACK;
        r.FrontCounterClockwise = false;  // CW = front (D3D11 default)
    }
    Diligent::IPipelineResourceSignature* pPRSArray[] = { pPRS.RawPtr() };
    psoci.ppResourceSignatures = pPRSArray;
    psoci.ResourceSignaturesCount = 1;

    if (info.useDepthStencilOverride) {
        psoci.GraphicsPipeline.DepthStencilDesc.DepthEnable = info.depthStencilOverride.depthTest;
        psoci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = info.depthStencilOverride.depthWrite;
    }

    Diligent::IPipelineState* pPSOPtr = nullptr;
    m_impl->pDevice->CreateGraphicsPipelineState(psoci, &pPSOPtr);
    DestroyShader(pRhiVs);
    DestroyShader(pRhiPs);
    if (pPSOPtr) {
        pPSOPtr->AddRef();
        p->m_pNative = pPSOPtr;
        p->m_shaderAssetPath = nameStr;
        Diligent::IShaderResourceBinding* pSRBPtr = nullptr;
        pPRS->CreateShaderResourceBinding(&pSRBPtr, true);
        if (pSRBPtr) {
            pSRBPtr->AddRef();
            p->m_pSRB = pSRBPtr;
        }
    }
    if (!p->m_pNative) {
        delete p;
        return nullptr;
    }
#endif
    return p;
}

IPipeline* DeviceDiligent::CreateGraphicsPipelineFromBinary(const void* data, size_t size, uint32_t binaryFormat, const VertexLayout& layout, PrimitiveTopology topology) {
    (void)data;
    (void)size;
    (void)binaryFormat;
    (void)layout;
    (void)topology;
    return nullptr;
}

bool DeviceDiligent::GetPipelineBinary(IPipeline* pipeline, std::vector<uint8_t>& outData, uint32_t& outFormat) {
    (void)pipeline;
    (void)outData;
    (void)outFormat;
    return false;
}

IPipeline* DeviceDiligent::CreateComputePipeline(void* computeShader) {
    (void)computeShader;
    return new PipelineDiligent();
}

void DeviceDiligent::DestroyPipeline(IPipeline* pipeline) {
    if (pipeline)
        delete static_cast<PipelineDiligent*>(pipeline);
}

IRenderTarget* DeviceDiligent::CreateRenderTarget(uint32_t width, uint32_t height,
                                                   const std::vector<RHITextureFormat>& colorFormats,
                                                   RHITextureFormat depthFormat,
                                                   uint32_t samples,
                                                   bool transient) {
    (void)samples;
    (void)transient;
    auto* rt = new RenderTargetDiligent();
    rt->m_width = width;
    rt->m_height = height;
#if OPENEYE_USE_DILIGENT
    if (m_impl && m_impl->pDevice) {
        using namespace Diligent;
        for (RHITextureFormat fmt : colorFormats) {
            auto tex = std::make_unique<TextureDiligent>();
            tex->m_width = width;
            tex->m_height = height;
            tex->m_format = fmt;
            Diligent::TextureDesc dgDesc;
            dgDesc.Type = RESOURCE_DIM_TEX_2D;
            dgDesc.Width = width;
            dgDesc.Height = height;
            dgDesc.Format = RHIToDiligentTextureFormat(fmt);
            dgDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
            dgDesc.Usage = USAGE_DEFAULT;
            Diligent::ITexture* pTex = nullptr;
            m_impl->pDevice->CreateTexture(dgDesc, nullptr, &pTex);
            if (pTex) {
                pTex->AddRef();
                tex->m_pNative = pTex;
                Diligent::ITextureView* pRTV = pTex->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
                if (pRTV) { pRTV->AddRef(); tex->m_pRTV = pRTV; }
                Diligent::ITextureView* pSRV = pTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
                if (pSRV) { pSRV->AddRef(); tex->m_pSRV = pSRV; }
            }
            rt->m_colorTextures.push_back(std::move(tex));
        }
        if (depthFormat != RHITextureFormat::None) {
            rt->m_depthTexture = std::make_unique<TextureDiligent>();
            rt->m_depthTexture->m_width = width;
            rt->m_depthTexture->m_height = height;
            rt->m_depthTexture->m_format = depthFormat;
            Diligent::TextureDesc dgDesc;
            dgDesc.Type = RESOURCE_DIM_TEX_2D;
            dgDesc.Width = width;
            dgDesc.Height = height;
            dgDesc.Format = RHIToDiligentTextureFormat(depthFormat);
            dgDesc.BindFlags = BIND_DEPTH_STENCIL;
            dgDesc.Usage = USAGE_DEFAULT;
            Diligent::ITexture* pTex = nullptr;
            m_impl->pDevice->CreateTexture(dgDesc, nullptr, &pTex);
            if (pTex) {
                pTex->AddRef();
                rt->m_depthTexture->m_pNative = pTex;
                Diligent::ITextureView* pDSV = pTex->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
                if (pDSV) { pDSV->AddRef(); rt->m_depthTexture->m_pDSV = pDSV; }
            }
        }
    } else
#endif
    {
        for (RHITextureFormat fmt : colorFormats) {
            auto tex = std::make_unique<TextureDiligent>();
            tex->m_width = width;
            tex->m_height = height;
            tex->m_format = fmt;
            rt->m_colorTextures.push_back(std::move(tex));
        }
        if (depthFormat != RHITextureFormat::None) {
            rt->m_depthTexture = std::make_unique<TextureDiligent>();
            rt->m_depthTexture->m_width = width;
            rt->m_depthTexture->m_height = height;
            rt->m_depthTexture->m_format = depthFormat;
        }
    }
    return rt;
}

IRenderTarget* DeviceDiligent::CreateRenderTargetWithSharedDepth(uint32_t width, uint32_t height,
                                                                 const std::vector<RHITextureFormat>& colorFormats,
                                                                 ITexture* sharedDepthTexture,
                                                                 uint32_t samples) {
    (void)width;
    (void)height;
    (void)colorFormats;
    (void)sharedDepthTexture;
    (void)samples;
    return nullptr;  // TODO: implement for Diligent when needed (editor uses OpenGL)
}

IRenderTarget* DeviceDiligent::CreateRenderTargetFromTexture(ITexture* colorTexture, uint32_t face, uint32_t mipLevel) {
    (void)face;
    (void)mipLevel;
    if (!colorTexture) return nullptr;
    auto* rt = new RenderTargetDiligent();
    rt->m_width = colorTexture->GetWidth();
    rt->m_height = colorTexture->GetHeight();
    rt->m_externalColorTexture = colorTexture;
    return rt;
}

void DeviceDiligent::DestroyRenderTarget(IRenderTarget* rt) {
    if (rt)
        delete static_cast<RenderTargetDiligent*>(rt);
}

IQueryPool* DeviceDiligent::CreateQueryPool(QueryType type, uint32_t count) {
    auto* q = new QueryPoolDiligent();
    q->m_type = type;
    q->m_count = count;
    return q;
}

void DeviceDiligent::DestroyQueryPool(IQueryPool* pool) {
    if (pool)
        delete static_cast<QueryPoolDiligent*>(pool);
}

IPipelineLayout* DeviceDiligent::CreatePipelineLayout(const PipelineLayoutDesc& desc) {
    auto* layout = new PipelineLayoutDiligent();
    layout->desc = desc;
    return layout;
}

void DeviceDiligent::DestroyPipelineLayout(IPipelineLayout* layout) {
    delete static_cast<PipelineLayoutDiligent*>(layout);
}

IDescriptorSet* DeviceDiligent::CreateDescriptorSet(IPipelineLayout* layout) {
    if (!layout) return nullptr;
    auto* pl = static_cast<PipelineLayoutDiligent*>(layout);
    auto* set = new DescriptorSetDiligent();
    set->layout = pl;
    return set;
}

void DeviceDiligent::DestroyDescriptorSet(IDescriptorSet* set) {
    delete static_cast<DescriptorSetDiligent*>(set);
}

IBindlessHeap* DeviceDiligent::CreateBindlessHeap() {
    if (!backend::SupportsBindless(this)) return nullptr;
    return nullptr;  // Skeleton: full implementation (e.g. descriptor indexing) deferred
}

void DeviceDiligent::DestroyBindlessHeap(IBindlessHeap* heap) {
    (void)heap;
}

int DeviceDiligent::GetMaxSamples(int requestedSamples) {
    (void)requestedSamples;
    return 1;
}

ITexture* DeviceDiligent::CreateTextureWrapper(void* nativeTextureId, uint32_t width, uint32_t height, RHITextureFormat format) {
    (void)nativeTextureId;
    auto* t = new TextureDiligent();
    t->m_width = width;
    t->m_height = height;
    t->m_format = format;
    return t;
}

#if OPENEYE_USE_DILIGENT
void DeviceDiligent::EnsureBlitResources() {
    if (!m_impl || !m_impl->pDevice) {
        static bool once;
        if (!once) { core::Log::Warn("Diligent EnsureBlitResources: skip (no impl/device)"); once = true; }
        return;
    }
    if (m_blitPipeline) return;
    using namespace Diligent;
    VertexLayout emptyLayout;
    ShaderGraphicsPipelineCreateInfo blitInfo{};
    blitInfo.assetPath = "passes/post/copy";
    blitInfo.vertexLayout = emptyLayout;
    blitInfo.topology = PrimitiveTopology::TriangleList;
    blitInfo.layoutKind = PipelineLayoutKind::Post;
    blitInfo.attachmentFormats = StandardAttachmentFormats::PostCopyBGRA();
    m_blitPipeline = CreateGraphicsPipelineFromShaderAsset(blitInfo);
    if (!m_blitPipeline || !static_cast<PipelineDiligent*>(m_blitPipeline)->HasValidProgram()) {
        core::Log::Warn("Diligent EnsureBlitResources: failed (copy pipeline null or invalid program)");
        return;
    }
    struct PosTexVertex { float x, y, u, v; };
    const PosTexVertex quadVerts[] = {
        {-1.f, -1.f, 0.f, 1.f},
        { 1.f, -1.f, 1.f, 1.f},
        {-1.f,  1.f, 0.f, 0.f},
        { 1.f,  1.f, 1.f, 0.f},
    };
    const uint16_t quadIndices[] = { 0, 1, 2, 1, 3, 2 };
    BufferDesc vbDesc;
    vbDesc.sizeInBytes = sizeof(quadVerts);
    vbDesc.usage = BufferUsage::Vertex;
    vbDesc.memoryHint = BufferMemoryHint::DeviceOnly;
    m_blitVB = CreateBuffer(vbDesc, quadVerts);
    BufferDesc ibDesc;
    ibDesc.sizeInBytes = sizeof(quadIndices);
    ibDesc.usage = BufferUsage::Index;
    ibDesc.memoryHint = BufferMemoryHint::DeviceOnly;
    m_blitIB = CreateBuffer(ibDesc, quadIndices);
    SamplerDesc sampDesc;
    sampDesc.MinFilter = FILTER_TYPE_LINEAR;
    sampDesc.MagFilter = FILTER_TYPE_LINEAR;
    sampDesc.MipFilter = FILTER_TYPE_LINEAR;
    sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = TEXTURE_ADDRESS_CLAMP;
    ISampler* pSamp = nullptr;
    m_impl->pDevice->CreateSampler(sampDesc, &pSamp);
    if (!pSamp) {
        core::Log::Warn("Diligent EnsureBlitResources: failed (CreateSampler returned null)");
        return;
    }
    pSamp->AddRef();
    m_blitSampler = pSamp;
    core::Log::Info("Diligent EnsureBlitResources: OK (pipeline+VB+IB+sampler)");
}

void DeviceDiligent::BlitToDefaultFramebuffer(IRenderTarget* src) {
    static int s_blitSkipReason = -1;  // -1=none, 0=done, 1=no src/impl/ctx, 2=no pipe/vb/ib/samp, 3=no color0, 4=no SRV, 5=no PSO/SRB, 6=no RTV, 7=no VB/IB native
    if (!src || !m_impl || !m_impl->pContext) {
        if (s_blitSkipReason != 1) { core::Log::Warn("Diligent BlitToDefaultFramebuffer: skip (no src or impl or context)"); s_blitSkipReason = 1; }
        return;
    }
    EnsureBlitResources();
    if (!m_blitPipeline || !m_blitVB || !m_blitIB || !m_blitSampler) {
        if (s_blitSkipReason != 2) { core::Log::Warn("Diligent BlitToDefaultFramebuffer: skip (no pipeline/VB/IB/sampler)"); s_blitSkipReason = 2; }
        return;
    }
    ITexture* color0 = src->GetColorTexture(0);
    if (!color0) {
        if (s_blitSkipReason != 3) { core::Log::Warn("Diligent BlitToDefaultFramebuffer: skip (no color texture 0)"); s_blitSkipReason = 3; }
        return;
    }
    TextureDiligent* tex = static_cast<TextureDiligent*>(color0);
    if (!tex->m_pSRV) {
        if (s_blitSkipReason != 4) { core::Log::Warn("Diligent BlitToDefaultFramebuffer: skip (color texture has no SRV)"); s_blitSkipReason = 4; }
        return;
    }
    Diligent::IDeviceContext* ctx = m_impl->pContext.RawPtr();
    PipelineDiligent* pl = static_cast<PipelineDiligent*>(m_blitPipeline);
    if (!pl->m_pNative || !pl->m_pSRB) {
        if (s_blitSkipReason != 5) { core::Log::Warn("Diligent BlitToDefaultFramebuffer: skip (no PSO or SRB)"); s_blitSkipReason = 5; }
        return;
    }
    Diligent::ITextureView* pRTV = static_cast<Diligent::ITextureView*>(GetSwapChainBackBufferRTV());
    if (!pRTV) {
        if (s_blitSkipReason != 6) { core::Log::Warn("Diligent BlitToDefaultFramebuffer: skip (no swap chain RTV)"); s_blitSkipReason = 6; }
        return;
    }
    // Do not transition the source texture here: it may already be SHADER_RESOURCE from the post chain.
    // CommitShaderResources(TRANSITION) below will transition it to SHADER_RESOURCE if needed.
    ctx->SetRenderTargets(1, &pRTV, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::Viewport vp;
    vp.TopLeftX = vp.TopLeftY = 0.f;
    vp.Width = static_cast<float>(m_width);
    vp.Height = static_cast<float>(m_height);
    vp.MinDepth = 0.f; vp.MaxDepth = 1.f;
    ctx->SetViewports(1, &vp, m_width, m_height);
    ctx->SetPipelineState(static_cast<Diligent::IPipelineState*>(pl->m_pNative));
    Diligent::IShaderResourceBinding* pSRB = static_cast<Diligent::IShaderResourceBinding*>(pl->m_pSRB);
    if (auto* vTex = pSRB->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_Texture"))
        vTex->Set(static_cast<Diligent::ITextureView*>(tex->m_pSRV));
    if (m_blitSampler) {
        if (auto* vSamp = pSRB->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_Texture_sampler"))
            vSamp->Set(static_cast<Diligent::ISampler*>(m_blitSampler));
    }
    ctx->CommitShaderResources(pSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::IBuffer* pVB = static_cast<Diligent::IBuffer*>(static_cast<BufferDiligent*>(m_blitVB)->m_pNative);
    Diligent::IBuffer* pIB = static_cast<Diligent::IBuffer*>(static_cast<BufferDiligent*>(m_blitIB)->m_pNative);
    if (!pVB || !pIB) {
        if (s_blitSkipReason != 7) { core::Log::Warn("Diligent BlitToDefaultFramebuffer: skip (blit VB/IB native null)"); s_blitSkipReason = 7; }
        return;
    }
    if (s_blitSkipReason != 0) { core::Log::Info("Diligent BlitToDefaultFramebuffer: drawing fullscreen quad"); s_blitSkipReason = 0; }
    Diligent::Uint64 voff = 0;
    ctx->SetVertexBuffers(0, 1, &pVB, &voff, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->SetIndexBuffer(pIB, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Diligent::DrawIndexedAttribs attr;
    attr.NumIndices = 6;
    attr.IndexType = Diligent::VT_UINT16;
    ctx->DrawIndexed(attr);
}
#else
void DeviceDiligent::EnsureBlitResources() {}
void DeviceDiligent::BlitToDefaultFramebuffer(IRenderTarget* src) { (void)src; }
#endif

void DeviceDiligent::Present() {
#if OPENEYE_USE_DILIGENT
    if (m_impl && m_impl->pSwapChain)
        m_impl->pSwapChain->Present();
#endif
}


void DeviceDiligent::BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) {
    (void)nativeFbo;
    (void)width;
    (void)height;
}

void DeviceDiligent::BlitToDefaultFramebufferSubregion(IRenderTarget* src, int dstX, int dstY, int dstW, int dstH) {
    (void)dstX;
    (void)dstY;
    (void)dstW;
    (void)dstH;
    BlitToDefaultFramebuffer(src);
}

void DeviceDiligent::BlitToDefaultFramebufferLetterbox(IRenderTarget* src, uint32_t windowWidth, uint32_t windowHeight) {
    (void)windowWidth;
    (void)windowHeight;
    BlitToDefaultFramebuffer(src);
}

void DeviceDiligent::Blit(IRenderTarget* src, IRenderTarget* dst, bool colorOnly) {
    (void)src;
    (void)dst;
    (void)colorOnly;
}

void DeviceDiligent::BlitFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst, bool colorOnly) {
    (void)nativeFbo;
    (void)dst;
    (void)colorOnly;
}

void DeviceDiligent::BlitDepthOnly(IRenderTarget* src, IRenderTarget* dst) {
    (void)src;
    (void)dst;
}

bool DeviceDiligent::ReadPixelUint32(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t* outValue) {
    (void)rt;
    (void)x;
    (void)y;
    (void)outValue;
    return false;  // TODO: implement for Diligent when needed
}

bool DeviceDiligent::ReadPixelsRGBA8(IRenderTarget* rt, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t* outBuffer) {
    (void)rt;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)outBuffer;
    return false;  // TODO: implement for Diligent when needed
}

} // namespace rhi
} // namespace render
} // namespace eyelib
