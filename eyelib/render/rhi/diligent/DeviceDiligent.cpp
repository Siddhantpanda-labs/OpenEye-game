#include "eyelib/render/rhi/diligent/DeviceDiligent.h"
#include "eyelib/render/rhi/diligent/BufferDiligent.h"
#include "eyelib/render/rhi/diligent/TextureDiligent.h"
#include "eyelib/render/rhi/diligent/ShaderDiligent.h"
#include "eyelib/render/rhi/diligent/PipelineDiligent.h"
#include "eyelib/render/rhi/diligent/RenderTargetDiligent.h"
#include "eyelib/render/rhi/diligent/QueryPoolDiligent.h"
#include "eyelib/render/rhi/RHITypes.h"
#include "eyelib/core/Log.h"
#include "eyelib/platform/filesystem/FileSystem.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#if OPENEYE_USE_DILIGENT
#include "eyelib/render/rhi/diligent/DeviceDiligentImpl.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/InputLayout.h"
#include "Graphics/GraphicsEngine/interface/PipelineResourceSignature.h"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Common/interface/RefCntAutoPtr.hpp"
#endif

namespace eyelib {
namespace render {
namespace rhi {

DeviceDiligent::DeviceDiligent(void* nwh, uint32_t width, uint32_t height, DeviceDiligentImpl* impl)
    : m_impl(impl), m_width(width), m_height(height) {
    (void)nwh;
    m_immediateCommandList = std::make_unique<CommandListDiligent>(this);
}

DeviceDiligent::~DeviceDiligent() {
    m_immediateCommandList.reset();
    m_impl.reset();
}

void* DeviceDiligent::GetImmediateContext() const {
#if OPENEYE_USE_DILIGENT
    if (m_impl && m_impl->pContext) return m_impl->pContext.RawPtr();
#endif
    return nullptr;
}

void* DeviceDiligent::GetSwapChainBackBufferRTV() const {
#if OPENEYE_USE_DILIGENT
    if (m_impl && m_impl->pSwapChain) return m_impl->pSwapChain->GetCurrentBackBufferRTV();
#endif
    return nullptr;
}

void* DeviceDiligent::GetDiligentDevice() const {
#if OPENEYE_USE_DILIGENT
    if (m_impl && m_impl->pDevice) return m_impl->pDevice.RawPtr();
#endif
    return nullptr;
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
        using namespace Diligent;
        const bool isVertex = HasUsage(desc.usage, BufferUsage::Vertex);
        const bool isIndex = HasUsage(desc.usage, BufferUsage::Index);
        if (isVertex || isIndex) {
            Diligent::BufferDesc dgDesc;
            dgDesc.Size = desc.sizeInBytes;
            dgDesc.Usage = Diligent::USAGE_DEFAULT;
            dgDesc.BindFlags = isVertex ? Diligent::BIND_VERTEX_BUFFER : Diligent::BIND_INDEX_BUFFER;
            Diligent::BufferData initData(initialData, initialData ? desc.sizeInBytes : 0, m_impl->pContext.RawPtr());
            Diligent::IBuffer* pNative = nullptr;
            m_impl->pDevice->CreateBuffer(dgDesc, initialData ? &initData : nullptr, &pNative);
            if (pNative) {
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
    if (!buffer) return;
    auto* b = static_cast<BufferDiligent*>(buffer);
#if OPENEYE_USE_DILIGENT
    if (b->m_pNative) {
        static_cast<Diligent::IBuffer*>(b->m_pNative)->Release();
        b->m_pNative = nullptr;
    }
#endif
    delete b;
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

IPipeline* DeviceDiligent::CreateGraphicsPipelineFromName(const char* shaderName, const VertexLayout& layout, PrimitiveTopology topology) {
    auto* p = new PipelineDiligent();
    p->m_vertexLayout = layout;
    p->m_topology = topology;

#if OPENEYE_USE_DILIGENT
    if (!m_impl || !m_impl->pDevice || !shaderName || !shaderName[0])
        return p;

    using namespace Diligent;
    using namespace eyelib::platform::filesystem;

    // Prefer exe directory (e.g. build/bin/Debug) so assets/shaders/diligent next to the exe is used
    std::string baseDir = "assets/shaders/diligent/";
    std::string vshPath, pshPath;
    std::string exeDir = GetDirectory(GetExecutablePath());
    if (!exeDir.empty() && FileExists(exeDir + baseDir + shaderName + ".vsh")) {
        vshPath = exeDir + baseDir + shaderName + ".vsh";
        pshPath = exeDir + baseDir + shaderName + ".psh";
    } else {
        vshPath = baseDir + shaderName + ".vsh";
        pshPath = baseDir + shaderName + ".psh";
    }
    if (!FileExists(vshPath) || !FileExists(pshPath))
        return p;

    std::vector<uint8_t> vsBuf = ReadFile(vshPath);
    std::vector<uint8_t> psBuf = ReadFile(pshPath);
    if (vsBuf.empty() || psBuf.empty())
        return p;
    vsBuf.push_back(0);
    psBuf.push_back(0);
    const char* vsSrc = reinterpret_cast<const char*>(vsBuf.data());
    const char* psSrc = reinterpret_cast<const char*>(psBuf.data());

    IShader* pRhiVs = CreateShader(ShaderStage::Vertex, vsSrc, vsBuf.size() - 1);
    IShader* pRhiPs = CreateShader(ShaderStage::Fragment, psSrc, psBuf.size() - 1);
    ShaderDiligent* pVs = static_cast<ShaderDiligent*>(pRhiVs);
    ShaderDiligent* pPs = static_cast<ShaderDiligent*>(pRhiPs);
    if (!pVs->IsValid() || !pPs->IsValid()) {
        DestroyShader(pRhiVs);
        DestroyShader(pRhiPs);
        return p;
    }

    Diligent::IShader* pDgVs = static_cast<Diligent::IShader*>(pVs->m_pNative);
    Diligent::IShader* pDgPs = static_cast<Diligent::IShader*>(pPs->m_pNative);

    // Input layout from VertexLayout or default fullscreen quad
    static constexpr Uint32 MAX_EL = 8u;
    LayoutElement layoutElements[MAX_EL];
    Uint32 numElements = 0;
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
    const std::string nameStr(shaderName);
    const bool isForward = (nameStr.find("passes/forward/") != std::string::npos);
    const bool isSkinned = (nameStr.find("skinned") != std::string::npos);
    std::vector<PipelineResourceDesc> resourceList;
    if (isForward) {
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_VERTEX, "Constants", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        if (isSkinned)
            resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_VERTEX, "Bones", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "Material", 1u, SHADER_RESOURCE_TYPE_CONSTANT_BUFFER, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "u_ShadowMap", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
    } else {
        resourceList.push_back(PipelineResourceDesc{SHADER_TYPE_PIXEL, "u_Texture", 1u, SHADER_RESOURCE_TYPE_TEXTURE_SRV, SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC});
    }
    PipelineResourceSignatureDesc prsDesc;
    prsDesc.Name = "ForwardPRS";
    prsDesc.Resources = resourceList.data();
    prsDesc.NumResources = static_cast<Uint32>(resourceList.size());
    prsDesc.UseCombinedTextureSamplers = true;
    prsDesc.CombinedSamplerSuffix = "_sampler";
    Diligent::IPipelineResourceSignature* pPRSPtr = nullptr;
    m_impl->pDevice->CreatePipelineResourceSignature(prsDesc, &pPRSPtr);
    RefCntAutoPtr<Diligent::IPipelineResourceSignature> pPRS;
    if (pPRSPtr) pPRS.Attach(pPRSPtr);
    if (!pPRS)
        return p;

    GraphicsPipelineStateCreateInfo psoci;
    psoci.PSODesc.Name = shaderName;
    psoci.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    psoci.pVS = pDgVs;
    psoci.pPS = pDgPs;
    psoci.GraphicsPipeline.InputLayout = inputLayoutDesc;
    psoci.GraphicsPipeline.PrimitiveTopology = (topology == PrimitiveTopology::TriangleList)
        ? PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
        : (topology == PrimitiveTopology::TriangleStrip)
            ? PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
            : (topology == PrimitiveTopology::PointList)
                ? PRIMITIVE_TOPOLOGY_POINT_LIST
                : PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoci.GraphicsPipeline.NumRenderTargets = 1;
    psoci.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM_SRGB;
    if (m_impl->pSwapChain) {
        const auto& scDesc = m_impl->pSwapChain->GetDesc();
        psoci.GraphicsPipeline.RTVFormats[0] = scDesc.ColorBufferFormat;
    }
    Diligent::IPipelineResourceSignature* pPRSArray[] = { pPRS.RawPtr() };
    psoci.ppResourceSignatures = pPRSArray;
    psoci.ResourceSignaturesCount = 1;

    Diligent::IPipelineState* pPSOPtr = nullptr;
    m_impl->pDevice->CreateGraphicsPipelineState(psoci, &pPSOPtr);
    DestroyShader(pRhiVs);
    DestroyShader(pRhiPs);
    if (pPSOPtr) {
        pPSOPtr->AddRef();
        p->m_pNative = pPSOPtr;
        Diligent::IShaderResourceBinding* pSRB = nullptr;
        pPRS->CreateShaderResourceBinding(&pSRB);
        if (pSRB) {
            p->m_pSRB = pSRB;
        }
    }
#endif
    return p;
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
                                                   uint32_t samples) {
    (void)samples;
    auto* rt = new RenderTargetDiligent();
    rt->m_width = width;
    rt->m_height = height;
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
    return rt;
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

void DeviceDiligent::Present() {
#if OPENEYE_USE_DILIGENT
    if (m_impl && m_impl->pSwapChain)
        m_impl->pSwapChain->Present();
#endif
}

void DeviceDiligent::BlitToDefaultFramebuffer(IRenderTarget* src) { (void)src; }

void DeviceDiligent::BlitToDefaultFramebuffer(void* nativeFbo, uint32_t width, uint32_t height) {
    (void)nativeFbo;
    (void)width;
    (void)height;
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

void DeviceDiligent::BlitDepthOnlyFromNativeFramebuffer(void* nativeFbo, IRenderTarget* dst) {
    (void)nativeFbo;
    (void)dst;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
