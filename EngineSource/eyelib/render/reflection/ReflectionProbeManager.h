#pragma once

#include "eyelib/render/reflection/ReflectionProbe.h"
#include "eyelib/render/RenderTypes.h"
#include "eyelib/render/rhi/RHI.h"
#include "eyelib/math/Types.h"
#include <functional>
#include <memory>
#include <vector>

namespace eyelib {
namespace render {

/**
 * Manages reflection probes: add probes, find closest by position, get global probe, capture from scene.
 */
class ReflectionProbeManager {
public:
    /** Callback: render scene (forward opaque + skybox) to rt using the given camera. */
    using RenderToTargetFn = std::function<void(rhi::ICommandList* cmd, const RenderFrameData& frameData, const CameraView& cam, rhi::IRenderTarget* rt)>;

    ReflectionProbeManager() = default;

    /** Add a probe; returns index. */
    size_t AddProbe(const ReflectionProbe& probe);
    /** Get probe by index. */
    ReflectionProbe* GetProbe(size_t index);
    const ReflectionProbe* GetProbe(size_t index) const;
    size_t GetProbeCount() const { return m_probes.size(); }

    /** Find closest probe to world position (within radius). Returns nullptr if none in range. */
    ReflectionProbe* FindClosestProbe(const math::Vec3& worldPos);
    const ReflectionProbe* FindClosestProbe(const math::Vec3& worldPos) const;

    /** Global probe (index 0). Use for single global environment (e.g. from skybox/IBL). */
    ReflectionProbe* GetGlobalProbe();
    const ReflectionProbe* GetGlobalProbe() const;

    /** Set global probe from external prefiltered/irradiance (e.g. IBL). Creates or updates probe at index 0. */
    void SetGlobalProbeFromPrefiltered(rhi::ITexture* prefilteredMap, rhi::ITexture* irradianceMap);

    /**
     * Capture scene to a probe: render 6 faces from position, prefilter, assign to probe.
     * renderToTarget(cmd, frameData, cam, rt) is called once per face. Creates/updates probe at probeIndex.
     * Resolution is the cubemap face size (e.g. 256). Manager owns the created textures.
     */
    bool CaptureProbe(rhi::IDevice* device, const RenderToTargetFn& renderToTarget, const RenderFrameData& frameData,
                      size_t probeIndex, const math::Vec3& position, uint32_t resolution = 256);

    void Clear();

private:
    struct OwnedProbeTextures {
        rhi::IDevice* device = nullptr;
        rhi::ITexture* cubemap = nullptr;
        rhi::ITexture* irradiance = nullptr;
        rhi::ITexture* prefilter = nullptr;
        ~OwnedProbeTextures();
    };
    std::vector<ReflectionProbe> m_probes;
    std::vector<std::unique_ptr<OwnedProbeTextures>> m_ownedTextures;
};

} // namespace render
} // namespace eyelib
