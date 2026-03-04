#pragma once

#include "api/render/RenderDevice.h"
#include <cstdint>
#include <memory>

namespace eyelib {
namespace render { class GBuffer; }
namespace api {

class GBuffer {
public:
    GBuffer();
    ~GBuffer();

    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;
    GBuffer(GBuffer&& other) noexcept;
    GBuffer& operator=(GBuffer&& other) noexcept;

    bool Initialize(uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);

    /** Set device for Bind* and Unbind. Call after creation when using this GBuffer (e.g. from pipeline). */
    void SetDevice(RenderDevice device);

    void BindForGeometryPass();
    void BindForLightingPass();
    void BindDepthTexture(uint32_t textureUnit);
    void Unbind();

    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    uint32_t GetPositionTexture() const;
    uint32_t GetNormalTexture() const;
    uint32_t GetAlbedoTexture() const;
    uint32_t GetPBRParamsTexture() const;
    uint32_t GetVelocityTexture() const;
    uint32_t GetDepthTexture() const;

    std::shared_ptr<::eyelib::render::GBuffer> GetInternal() const;
    static std::shared_ptr<GBuffer> WrapInternal(const std::shared_ptr<::eyelib::render::GBuffer>& buffer);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
