#pragma once

#include "eyelib/vr/XRTypes.h"

#include <cstdint>

namespace eyelib {
namespace vr {

/**
 * XR provider interface: session lifecycle, view data, and frame submit.
 * Implementations (e.g. OpenXR plugin) are decoupled from the engine;
 * only this interface is used by the render path.
 */
class IXRProvider {
public:
    virtual ~IXRProvider() = default;

    /** One-time init (e.g. load runtime). */
    virtual bool Init() = 0;
    virtual void Shutdown() = 0;

    /** Create session; graphicsBinding is API-specific (e.g. OpenGL context). */
    virtual bool CreateSession(void* graphicsBinding) = 0;
    virtual void DestroySession() = 0;
    virtual bool IsSessionActive() const = 0;

    /** Number of views (e.g. 2 for stereo). Valid when session is active. */
    virtual uint32_t GetViewCount() const = 0;

    /** Per-view pose and FOV for the current frame. Call after BeginFrame(). */
    virtual bool GetViewPoseFov(uint32_t viewIndex, XRViewData& out) const = 0;

    /** Recommended render size per view (width x height). */
    virtual void GetRecommendedViewSize(uint32_t viewIndex, uint32_t& width, uint32_t& height) const = 0;

    /** Start frame; returns false if we should not render (e.g. session lost). */
    virtual bool BeginFrame() = 0;
    /** End frame and submit. textureHandles[] per view (e.g. OpenGL texture names). */
    virtual bool EndFrame(uint32_t viewCount, const XRTextureHandle* textureHandles) = 0;
};

} // namespace vr
} // namespace eyelib
