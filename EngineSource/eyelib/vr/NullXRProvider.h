#pragma once

#include "eyelib/vr/IXRProvider.h"

namespace eyelib {
namespace vr {

/** Stub XR provider: no session, no views. Use when no VR runtime is available. */
class NullXRProvider : public IXRProvider {
public:
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
};

} // namespace vr
} // namespace eyelib
