#pragma once

#include "eyelib/rendering/framegraph/FrameGraphPass.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace rendering {
namespace rhi {
struct IDevice;
struct ICommandList;
}

class FrameGraphBlackboard;

/** Declarative pass container; compile/execute grow with barrier policy (doc §1, §5). */
class FrameGraph {
public:
    FrameGraph() = default;

    void Clear() noexcept;

    void AddPass(std::unique_ptr<IFrameGraphPass> pass);

    /**
     * Barrier ordering / pass validation (M3: assigns pass indices; future: resource transitions).
     * Optional `device` reserved for barrier-tracker queries without a command list.
     */
    void Compile(rhi::IDevice* device = nullptr);

    /**
     * For each pass: optional `CollectTextureBarriers` → transitions → `FlushBarrierTracker` → `Execute`.
     * `blackboard` may be null (skips barrier collection).
     */
    void Execute(rhi::IDevice* device, rhi::ICommandList* cmd, const FrameGraphBlackboard* blackboard);

private:
    std::vector<std::unique_ptr<IFrameGraphPass>> m_passes;
};

}  // namespace rendering
}  // namespace eyelib
