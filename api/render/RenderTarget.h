#pragma once

#include "api/render/TextureTypes.h"
#include "api/render/Texture.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace eyelib {
namespace render { class RenderTarget; }
namespace api {

class RenderTarget {
public:
    RenderTarget();
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;
    RenderTarget(RenderTarget&& other) noexcept;
    RenderTarget& operator=(RenderTarget&& other) noexcept;

    bool Create(uint32_t width, uint32_t height,
                const std::vector<TextureFormat>& colorFormats,
                TextureFormat depthFormat = TextureFormat::Depth24,
                int samples = 0);

    void Resize(uint32_t width, uint32_t height);
    void Destroy();

    void Bind() const;
    void Unbind() const;
    void Resolve();

    uint32_t GetWidth() const;
    uint32_t GetHeight() const;
    int GetSamples() const;
    size_t GetColorAttachmentCount() const;

    std::shared_ptr<Texture> GetColorTexture(size_t slot = 0) const;
    std::shared_ptr<Texture> GetDepthTexture() const;

    static void Blit(const RenderTarget& source, const RenderTarget& dest, bool colorOnly = true);
    static int GetMaxSamples(int samples);

    static RenderTarget CreateColorDepth(uint32_t width, uint32_t height,
                                         TextureFormat colorFormat = TextureFormat::RGBA16F,
                                         TextureFormat depthFormat = TextureFormat::Depth24,
                                         int samples = 0);
    static RenderTarget CreateColorOnly(uint32_t width, uint32_t height,
                                        TextureFormat colorFormat = TextureFormat::RGBA16F,
                                        int samples = 0);
    static RenderTarget CreateDepthOnly(uint32_t width, uint32_t height,
                                        TextureFormat depthFormat = TextureFormat::Depth24,
                                        int samples = 0);

    std::shared_ptr<::eyelib::render::RenderTarget> GetInternal() const;
    static std::shared_ptr<RenderTarget> WrapInternal(const std::shared_ptr<::eyelib::render::RenderTarget>& target);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
