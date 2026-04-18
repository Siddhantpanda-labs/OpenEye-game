#pragma once

#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
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
     * Assigns pass indices (`OnCompiled`), builds per-pass texture barrier plans from `CollectTextureBarriers`
     * when `blackboard` is non-null, dedupes per pass, then merges against the device barrier tracker (cross-pass noops).
     * Debug (A.7): intra-pass conflicting `toState` warn; post-merge asserts on explicit `toState` / sim map; inter-pass warns
     * (late first barrier with Undefined tracker; simulated layout gap with no merged transitions between passes).
     * Optional `device` without tracker skips merge/validation.
     */
    void Compile(rhi::IDevice* device = nullptr, const FrameGraphBlackboard* blackboard = nullptr);

    /**
     * For each pass: replay compiled barrier plan → `FlushBarrierTracker` → `Execute`.
     * `blackboard` is retained for API compatibility (e.g. future execute-time validation); barriers use the compile plan.
     */
    void Execute(rhi::IDevice* device, rhi::ICommandList* cmd, const FrameGraphBlackboard* blackboard);

private:
    /** Cross-pass: drop transitions whose target already matches tracker/simulated layout (requires barrier tracker). */
    void MergeBarrierPlansAgainstTracker(rhi::IDevice* device);

    std::vector<std::unique_ptr<IFrameGraphPass>> m_passes;
    std::vector<FrameGraphBarrierList> m_barrierPlans;
};

}  // namespace rendering
}  // namespace eyelib
