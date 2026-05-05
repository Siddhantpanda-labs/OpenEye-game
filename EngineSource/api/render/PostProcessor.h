#pragma once

#include <cstdint>
#include <memory>

namespace eyelib {
namespace api {

/**
 * @brief Post-processor API (opaque). Apply effects like bloom, tone mapping, etc.
 * No OpenGL types exposed.
 */
class PostProcessor {
public:
    struct Settings {
        bool bloom = false;
        bool toneMapping = true;
        bool fxaa = false;
        bool ssao = false;
        bool ssr = false;
        bool motionBlur = false;
        float bloomIntensity = 1.0f;
        float exposure = 1.0f;
        // ... other settings
    };

    PostProcessor();
    ~PostProcessor();

    PostProcessor(const PostProcessor&) = delete;
    PostProcessor& operator=(const PostProcessor&) = delete;
    PostProcessor(PostProcessor&& other) noexcept;
    PostProcessor& operator=(PostProcessor&& other) noexcept;

    bool Initialize(uint32_t width, uint32_t height);
    void Resize(uint32_t width, uint32_t height);
    void Shutdown();

    /** Apply post-processing. Returns output texture ID (opaque). */
    uint32_t Process(uint32_t inputTexture, const Settings& settings);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
