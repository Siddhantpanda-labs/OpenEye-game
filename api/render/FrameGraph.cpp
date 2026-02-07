#include "api/render/FrameGraph.h"
#include "eyelib/render/framegraph/FrameGraph.h"
#include "eyelib/render/rhi/RHITypes.h"
#include <memory>

namespace eyelib {
namespace api {

namespace {
using namespace eyelib::render::rhi;
RHITextureFormat ToRHIFormat(TextureFormat format, bool /*srgb*/) {
    switch (format) {
        case TextureFormat::R8: return RHITextureFormat::R8;
        case TextureFormat::RG8: return RHITextureFormat::RG8;
        case TextureFormat::RGB8: return RHITextureFormat::RGB8;
        case TextureFormat::RGBA8: return RHITextureFormat::RGBA8;
        case TextureFormat::RGB16F: return RHITextureFormat::RGB16F;
        case TextureFormat::RGBA16F: return RHITextureFormat::RGBA16F;
        case TextureFormat::Depth24: return RHITextureFormat::Depth24;
        case TextureFormat::Depth24Stencil8: return RHITextureFormat::Depth24Stencil8;
        case TextureFormat::None: return RHITextureFormat::RGBA8;
    }
    return RHITextureFormat::RGBA8;
}
} // namespace

class FrameGraph::Impl {
public:
    std::unique_ptr<::eyelib::render::FrameGraph> fg = std::make_unique<::eyelib::render::FrameGraph>();
    std::vector<PassInfo> cachedPasses;
};

uint32_t FGPassResources::GetTexture(FGTextureHandle handle) const {
    return m_getTexture ? m_getTexture(handle) : 0;
}

unsigned FGPassResources::GetWidth() const {
    return m_width;
}

unsigned FGPassResources::GetHeight() const {
    return m_height;
}

FrameGraph::FrameGraph() : m_impl(std::make_unique<Impl>()) {}

FrameGraph::~FrameGraph() = default;

FrameGraph::FrameGraph(FrameGraph&& other) noexcept = default;

FrameGraph& FrameGraph::operator=(FrameGraph&& other) noexcept = default;

FGTextureHandle FrameGraph::CreateTexture(const FGTextureDesc& desc) {
    ::eyelib::render::FGTextureDesc internal;
    internal.width = desc.width;
    internal.height = desc.height;
    internal.format = ToRHIFormat(desc.format, desc.srgb);
    internal.srgb = desc.srgb;
    internal.name = desc.name;
    return m_impl->fg->CreateTexture(internal);
}

FGTextureHandle FrameGraph::ImportTexture(const std::string& name, uint32_t textureId, unsigned width, unsigned height) {
    return m_impl->fg->ImportTexture(name, textureId, width, height);
}

int FrameGraph::AddPass(const std::string& name, const ExecuteFunc& execute,
                        const std::vector<FGTextureHandle>& reads,
                        const std::vector<FGTextureHandle>& writes,
                        bool sideEffect) {
    return m_impl->fg->AddPass(name, [execute](const ::eyelib::render::FGPassResources& res) {
        FGPassResources apiRes;
        apiRes.m_getTexture = [&](FGTextureHandle h) -> uint32_t {
            return static_cast<uint32_t>(res.getTexture(h));
        };
        apiRes.m_width = res.width;
        apiRes.m_height = res.height;
        execute(apiRes);
    }, reads, writes, sideEffect);
}

void FrameGraph::Clear() {
    m_impl->fg->Clear();
}

void FrameGraph::Compile() {
    m_impl->fg->Compile();
}

void FrameGraph::Execute() {
    m_impl->fg->Execute();
    m_impl->cachedPasses.clear();
    for (const auto& p : m_impl->fg->GetPasses()) {
        PassInfo info;
        info.name = p.name;
        info.gpuMs = p.gpuMs;
        info.culled = p.culled;
        m_impl->cachedPasses.push_back(std::move(info));
    }
}

const std::vector<FrameGraph::PassInfo>& FrameGraph::GetPasses() const {
    return m_impl->cachedPasses;
}

uint32_t FrameGraph::Resolve(FGTextureHandle handle) const {
    return static_cast<uint32_t>(m_impl->fg->Resolve(handle));
}

unsigned FrameGraph::GetWidth() const {
    return m_impl->fg->GetWidth();
}

unsigned FrameGraph::GetHeight() const {
    return m_impl->fg->GetHeight();
}

void FrameGraph::SetFrameSize(unsigned w, unsigned h) {
    m_impl->fg->SetFrameSize(w, h);
}

} // namespace api
} // namespace eyelib
