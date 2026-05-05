#pragma once

#include "eyelib/math/Types.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace render { class CascadeShadowMap; }
namespace api {

struct RenderDevice;

class CascadeShadowMap {
public:
    CascadeShadowMap();
    ~CascadeShadowMap();

    CascadeShadowMap(const CascadeShadowMap&) = delete;
    CascadeShadowMap& operator=(const CascadeShadowMap&) = delete;
    CascadeShadowMap(CascadeShadowMap&& other) noexcept;
    CascadeShadowMap& operator=(CascadeShadowMap&& other) noexcept;

    /** Initialize with device (from RenderPipeline::GetDevice()), resolution and cascade count. */
    bool Initialize(RenderDevice device, uint32_t resolution = 2048, uint32_t splitCount = 4);

    /** Light-space matrix for cascade i. Call after Update. */
    const math::Mat4& GetLightSpaceMatrix(uint32_t cascadeIndex) const;
    uint32_t GetResolution() const;
    uint32_t GetSplitCount() const;

    ::eyelib::render::CascadeShadowMap* GetInternal() const;
    /** Wrap pipeline's cascade map (non-owning) for use with ShadowPass::Render. */
    static std::shared_ptr<CascadeShadowMap> WrapInternal(::eyelib::render::CascadeShadowMap* csm);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
