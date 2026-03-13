#include "api/render/PostProcessor.h"
#include "eyelib/render/post/PostProcessor.h"

namespace eyelib {
namespace api {

class PostProcessor::Impl {
public:
    std::unique_ptr<::eyelib::render::PostProcessor> processor = std::make_unique<::eyelib::render::PostProcessor>();
};

PostProcessor::PostProcessor() : m_impl(std::make_unique<Impl>()) {}

PostProcessor::~PostProcessor() = default;

PostProcessor::PostProcessor(PostProcessor&& other) noexcept = default;

PostProcessor& PostProcessor::operator=(PostProcessor&& other) noexcept = default;

bool PostProcessor::Initialize(uint32_t width, uint32_t height) {
    return m_impl->processor->Initialize(width, height);
}

void PostProcessor::Resize(uint32_t width, uint32_t height) {
    m_impl->processor->Resize(width, height);
}

void PostProcessor::Shutdown() {
    m_impl->processor->Shutdown();
}

uint32_t PostProcessor::Process(uint32_t inputTexture, const Settings& settings) {
    ::eyelib::render::PostProcessor::Settings s;
    s.enableBloom = settings.bloom;
    s.useACES = settings.toneMapping;
    s.enableFXAA = settings.fxaa;
    s.enableSSAO = settings.ssao;
    s.enableSSR = settings.ssr;
    s.enableMotionBlur = settings.motionBlur;
    s.bloomIntensity = settings.bloomIntensity;
    s.exposure = settings.exposure;
    // TODO: Call processor->ApplyPostProcessing with proper parameters
    return 0; // Placeholder
}

} // namespace api
} // namespace eyelib
