#include "eyelib/render/rhi/Vulkan/VulkanPipelineHash.h"
#include <cstring>

namespace eyelib {
namespace render {
namespace rhi {

namespace {

inline uint64_t HashCombine(uint64_t seed, uint64_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2));
}

} // namespace

uint64_t HashPipelineDesc(const PipelineDesc& desc) {
    uint64_t h = 0;
    h = HashCombine(h, reinterpret_cast<uintptr_t>(desc.vertexShader));
    h = HashCombine(h, reinterpret_cast<uintptr_t>(desc.fragmentShader));
    h = HashCombine(h, reinterpret_cast<uintptr_t>(desc.computeShader));
    h = HashCombine(h, reinterpret_cast<uintptr_t>(desc.tessControlShader));
    h = HashCombine(h, reinterpret_cast<uintptr_t>(desc.tessEvalShader));
    h = HashCombine(h, reinterpret_cast<uintptr_t>(desc.geometryShader));
    h = HashCombine(h, static_cast<uint64_t>(desc.vertexLayout.strideInBytes));
    for (const auto& a : desc.vertexLayout.attributes) {
        h = HashCombine(h, static_cast<uint64_t>(a.location));
        h = HashCombine(h, static_cast<uint64_t>(static_cast<uint32_t>(a.type)));
        h = HashCombine(h, static_cast<uint64_t>(a.offsetInBytes));
    }
    h = HashCombine(h, static_cast<uint64_t>(static_cast<uint32_t>(desc.topology)));
    h = HashCombine(h, static_cast<uint64_t>(desc.patchVertexCount));
    h = HashCombine(h, desc.blend.blendEnable ? 1u : 0u);
    h = HashCombine(h, static_cast<uint64_t>(static_cast<uint32_t>(desc.blend.srcColor)));
    h = HashCombine(h, static_cast<uint64_t>(static_cast<uint32_t>(desc.blend.dstColor)));
    h = HashCombine(h, static_cast<uint64_t>(static_cast<uint32_t>(desc.blend.colorOp)));
    h = HashCombine(h, static_cast<uint64_t>(static_cast<uint32_t>(desc.blend.srcAlpha)));
    h = HashCombine(h, static_cast<uint64_t>(static_cast<uint32_t>(desc.blend.dstAlpha)));
    h = HashCombine(h, static_cast<uint64_t>(static_cast<uint32_t>(desc.blend.alphaOp)));
    h = HashCombine(h, desc.depthStencil.depthTest ? 1u : 0u);
    h = HashCombine(h, desc.depthStencil.depthWrite ? 1u : 0u);
    h = HashCombine(h, static_cast<uint64_t>(static_cast<uint32_t>(desc.depthStencil.depthCompare)));
    h = HashCombine(h, desc.depthStencil.stencilTest ? 1u : 0u);
    h = HashCombine(h, desc.raster.cullEnable ? 1u : 0u);
    h = HashCombine(h, desc.raster.frontCounterClockwise ? 1u : 0u);
    h = HashCombine(h, desc.raster.depthBiasEnable ? 1u : 0u);
    uint32_t biasConst = 0, biasSlope = 0;
    std::memcpy(&biasConst, &desc.raster.depthBiasConstant, sizeof(float));
    std::memcpy(&biasSlope, &desc.raster.depthBiasSlope, sizeof(float));
    h = HashCombine(h, static_cast<uint64_t>(biasConst));
    h = HashCombine(h, static_cast<uint64_t>(biasSlope));
    h = HashCombine(h, static_cast<uint64_t>(desc.transformFeedbackVaryings.size()));
    for (const char* p : desc.transformFeedbackVaryings)
        h = HashCombine(h, p ? reinterpret_cast<uintptr_t>(p) : 0);
    return h;
}

} // namespace rhi
} // namespace render
} // namespace eyelib
