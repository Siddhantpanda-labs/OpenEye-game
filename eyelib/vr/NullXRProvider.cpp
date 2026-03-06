#include "eyelib/vr/NullXRProvider.h"

namespace eyelib {
namespace vr {

bool NullXRProvider::Init() {
    return true;
}

void NullXRProvider::Shutdown() {}

bool NullXRProvider::CreateSession(void* /*graphicsBinding*/) {
    return false;
}

void NullXRProvider::DestroySession() {}

bool NullXRProvider::IsSessionActive() const {
    return false;
}

uint32_t NullXRProvider::GetViewCount() const {
    return 0;
}

bool NullXRProvider::GetViewPoseFov(uint32_t /*viewIndex*/, XRViewData& /*out*/) const {
    return false;
}

void NullXRProvider::GetRecommendedViewSize(uint32_t /*viewIndex*/, uint32_t& width, uint32_t& height) const {
    width = height = 0;
}

bool NullXRProvider::BeginFrame() {
    return false;
}

bool NullXRProvider::EndFrame(uint32_t /*viewCount*/, const XRTextureHandle* /*textureHandles*/) {
    return false;
}

} // namespace vr
} // namespace eyelib
