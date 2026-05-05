#pragma once

#include "eyelib/math/Types.h"

#include <cstring>
#include <cstdint>

namespace eyelib {
namespace rendering {

/**
 * One instance row in the PerObject SSBO (std430). Matches future GLSL `PerObjectData`.
 * Column-major `model` matches `math::Mat4` / GLSL `mat4`.
 */
struct PerObjectGpuStd430 {
    float model[16]{};
    uint32_t materialId = 0;
    uint32_t meshHandle = 0;
    uint32_t transformIndex = 0;
    uint32_t _pad = 0;
};

static_assert(sizeof(PerObjectGpuStd430) == 80, "PerObjectGpuStd430 std430 packing");

inline void FillPerObjectGpuRow(const math::Mat4& worldMatrix, uint32_t materialId, uint32_t meshHandle, uint32_t transformIndex,
                                PerObjectGpuStd430& out) noexcept {
    std::memcpy(out.model, &worldMatrix[0][0], sizeof(out.model));
    out.materialId = materialId;
    out.meshHandle = meshHandle;
    out.transformIndex = transformIndex;
    out._pad = 0;
}

}  // namespace rendering
}  // namespace eyelib
