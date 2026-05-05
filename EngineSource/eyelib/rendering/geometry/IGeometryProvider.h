#pragma once

#include <cstdint>

namespace eyelib {
namespace rendering {
namespace rhi {
struct ICommandList;
}

struct DrawList;

/**
 * Consumes a CPU-visible `DrawList` slice and records indexed draws (doc `geometry/`).
 * **Does not** read lighting, shadow maps, or scene light buffers (doc §6).
 */
class IGeometryProvider {
public:
    virtual ~IGeometryProvider() = default;

    /**
     * Records draw commands for `drawList.items[beginIndex, beginIndex + count)`.
     * The active pass must have bound a compatible graphics pipeline and descriptor state before calling.
     */
    virtual void Submit(rhi::ICommandList* cmd, const DrawList& drawList, uint32_t beginIndex, uint32_t count) const = 0;
};

}  // namespace rendering
}  // namespace eyelib
