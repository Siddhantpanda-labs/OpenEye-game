#pragma once

#include "eyelib/vr/IXRProvider.h"

#include <cstdint>
#include <memory>

namespace eyelib {
namespace vr {

/** OpenXR implementation of IXRProvider. Use when a VR runtime (e.g. SteamVR) is available. */
class OpenXRProvider : public IXRProvider {
public:
    OpenXRProvider();
    ~OpenXRProvider() override;

    bool Init() override;
    void Shutdown() override;
    bool CreateSession(void* graphicsBinding) override;
    void DestroySession() override;
    bool IsSessionActive() const override;
    uint32_t GetViewCount() const override;
    bool GetViewPoseFov(uint32_t viewIndex, XRViewData& out) const override;
    void GetRecommendedViewSize(uint32_t viewIndex, uint32_t& width, uint32_t& height) const override;
    bool BeginFrame() override;
    bool EndFrame(uint32_t viewCount, const XRTextureHandle* textureHandles) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace vr
} // namespace eyelib
