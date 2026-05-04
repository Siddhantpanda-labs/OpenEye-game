#include "eyelib/rendering/framegraph/FrameGraph.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/core/GpuProfiler.h"
#include "eyelib/core/Log.h"
#include "eyelib/rendering/rhi/RHI.h"

#include <cassert>
#include <unordered_map>

namespace eyelib {
namespace rendering {

#if !defined(NDEBUG)
namespace {

void WarnIntrPassBarrierConflicts(const IFrameGraphPass* pass, uint32_t passIndex, const FrameGraphBarrierList& plan, int& warnBudget) {
    const auto& entries = plan.entries;
    for (size_t i = 0; i < entries.size(); ++i) {
        const FrameGraphBarrierList::Entry& a = entries[i];
        if (!a.texture)
            continue;
        for (size_t j = i + 1; j < entries.size(); ++j) {
            const FrameGraphBarrierList::Entry& b = entries[j];
            if (a.texture != b.texture)
                continue;
            if (a.toState == b.toState)
                continue;
            if (warnBudget <= 0)
                return;
            --warnBudget;
            const char* name = pass ? pass->DebugName() : "(null pass)";
            core::Log::WarnF(
                "FrameGraph::Compile: pass %u '%s' lists conflicting texture barriers for one texture (different toState in same flush batch).",
                static_cast<unsigned>(passIndex), name);
        }
    }
}

void ValidateMergedBarrierPlanDebug(rhi::IDevice* device, const std::vector<FrameGraphBarrierList>& plans) {
    if (!device || !device->GetBarrierTracker())
        return;
    std::unordered_map<const rhi::ITexture*, rhi::ResourceState> sim;
    for (size_t pi = 0; pi < plans.size(); ++pi) {
        for (const FrameGraphBarrierList::Entry& e : plans[pi].entries) {
            if (!e.texture)
                continue;
            assert(e.toState != rhi::ResourceState::Undefined && "FrameGraph: barrier toState must not be Undefined");
            auto it = sim.find(e.texture);
            if (it != sim.end()) {
                assert(it->second != rhi::ResourceState::Undefined &&
                       "FrameGraph: internal barrier sim state must not be Undefined");
            }
            sim[e.texture] = e.toState;
        }
    }
}

bool PlanTouchesTexture(const FrameGraphBarrierList& plan, const rhi::ITexture* tex) {
    if (!tex)
        return false;
    for (const FrameGraphBarrierList::Entry& e : plan.entries) {
        if (e.texture == tex)
            return true;
    }
    return false;
}

void WarnFirstMergedBarrierTextureUnknownLayout(rhi::IDevice* device, const std::vector<FrameGraphBarrierList>& merged,
                                               int& warnBudget) {
    if (!device || !device->GetBarrierTracker())
        return;
    std::unordered_map<const rhi::ITexture*, uint32_t> firstPass;
    for (size_t p = 0; p < merged.size(); ++p) {
        for (const FrameGraphBarrierList::Entry& e : merged[p].entries) {
            if (!e.texture)
                continue;
            if (firstPass.find(e.texture) != firstPass.end())
                continue;
            firstPass[e.texture] = static_cast<uint32_t>(p);
        }
    }
    for (const auto& kv : firstPass) {
        const rhi::ITexture* tex = kv.first;
        const uint32_t fp = kv.second;
        if (fp == 0)
            continue;
        if (device->GetTextureTrackedState(tex) != rhi::ResourceState::Undefined)
            continue;
        if (warnBudget <= 0)
            return;
        --warnBudget;
        core::Log::WarnF(
            "FrameGraph::Compile: first merged texture barrier is in pass %u but barrier-tracker layout is Undefined "
            "(no earlier pass in this graph established a known layout for this resource).",
            static_cast<unsigned>(fp));
    }
}

void WarnInterPassSimulatedLayoutGap(rhi::IDevice* device, const std::vector<FrameGraphBarrierList>& merged, int& warnBudget) {
    if (!device || !device->GetBarrierTracker())
        return;
    std::unordered_map<const rhi::ITexture*, rhi::ResourceState> sim;
    std::unordered_map<const rhi::ITexture*, uint32_t> lastPass;
    std::unordered_map<const rhi::ITexture*, rhi::ResourceState> lastTo;

    for (size_t p = 0; p < merged.size(); ++p) {
        for (const FrameGraphBarrierList::Entry& e : merged[p].entries) {
            if (!e.texture)
                continue;
            const rhi::ITexture* T = e.texture;
            rhi::ResourceState from = rhi::ResourceState::Undefined;
            const auto sit = sim.find(T);
            if (sit != sim.end())
                from = sit->second;
            else
                from = device->GetTextureTrackedState(T);

            const auto lpIt = lastPass.find(T);
            if (lpIt != lastPass.end()) {
                const uint32_t lp = lpIt->second;
                if (p > lp + 1) {
                    bool mergedTouchInGap = false;
                    for (size_t u = lp + 1; u < p; ++u) {
                        if (PlanTouchesTexture(merged[u], T)) {
                            mergedTouchInGap = true;
                            break;
                        }
                    }
                    const rhi::ResourceState expectedFrom = lastTo[T];
                    if (!mergedTouchInGap && from != expectedFrom && warnBudget > 0) {
                        --warnBudget;
                        core::Log::WarnF(
                            "FrameGraph::Compile: possible inter-pass layout disagreement for a texture (pass %u -> %u): "
                            "simulated layout before transition is %u but last merged transition left it at %u with no "
                            "merged barriers for this texture in between.",
                            static_cast<unsigned>(lp), static_cast<unsigned>(p), static_cast<unsigned>(from),
                            static_cast<unsigned>(expectedFrom));
                    }
                }
            }
            sim[T] = e.toState;
            lastPass[T] = static_cast<uint32_t>(p);
            lastTo[T] = e.toState;
        }
    }
}

}  // namespace
#endif

#if !defined(NDEBUG)
namespace {
struct DeclaredOpsRecord {
    RenderAttachmentOps ops{};
    uint32_t passIndex = 0;
    const char* passName = nullptr;
};

bool SameOps(const RenderAttachmentOps& a, const RenderAttachmentOps& b) {
    return a.hasDepth == b.hasDepth &&
           a.color.loadOp == b.color.loadOp &&
           a.color.storeOp == b.color.storeOp &&
           a.color.clearValue.rgba == b.color.clearValue.rgba &&
           a.depth.loadOp == b.depth.loadOp &&
           a.depth.storeOp == b.depth.storeOp &&
           a.depth.clearValue.depth == b.depth.clearValue.depth &&
           a.depth.clearValue.stencil == b.depth.clearValue.stencil;
}
}  // namespace
#endif

void FrameGraph::Clear() noexcept {
    m_passes.clear();
    m_borrowedPasses.clear();
    m_barrierPlans.clear();
}

void FrameGraph::AddPass(std::unique_ptr<IFrameGraphPass> pass) {
    if (pass)
        m_passes.push_back(std::move(pass));
}

void FrameGraph::AddPassBorrowed(IFrameGraphPass* pass) {
    if (pass)
        m_borrowedPasses.push_back(pass);
}

void FrameGraph::MergeBarrierPlansAgainstTracker(rhi::IDevice* device) {
    if (!device || !device->GetBarrierTracker())
        return;

    std::unordered_map<const rhi::ITexture*, rhi::ResourceState> layoutAfterPlan;

    for (size_t passIdx = 0; passIdx < m_barrierPlans.size(); ++passIdx) {
        FrameGraphBarrierList& plan = m_barrierPlans[passIdx];
        std::vector<FrameGraphBarrierList::Entry> compact;
        compact.reserve(plan.entries.size());
        for (const FrameGraphBarrierList::Entry& e : plan.entries) {
            if (!e.texture)
                continue;
            rhi::ResourceState cur = rhi::ResourceState::Undefined;
            const auto it = layoutAfterPlan.find(e.texture);
            if (it != layoutAfterPlan.end())
                cur = it->second;
            else
                cur = device->GetTextureTrackedState(e.texture);
            if (cur == e.toState)
                continue;
            compact.push_back(e);
            layoutAfterPlan[e.texture] = e.toState;
        }
        plan.entries = std::move(compact);
    }
}

void FrameGraph::Compile(rhi::IDevice* device, const FrameGraphBlackboard* blackboard) {
    // Build a unified pass view: owned first, then borrowed.
    std::vector<IFrameGraphPass*> allPasses;
    allPasses.reserve(m_passes.size() + m_borrowedPasses.size());
    for (auto& p : m_passes) if (p) allPasses.push_back(p.get());
    for (auto* p : m_borrowedPasses) if (p) allPasses.push_back(p);

    m_barrierPlans.resize(allPasses.size());
    for (uint32_t i = 0; i < allPasses.size(); ++i)
        allPasses[i]->OnCompiled(i);
    for (size_t i = 0; i < allPasses.size(); ++i) {
        FrameGraphBarrierList& plan = m_barrierPlans[i];
        plan.Clear();
        if (blackboard)
            allPasses[i]->CollectTextureBarriers(*blackboard, plan);
        plan.DedupeConsecutiveSameTargetState();
    }
#if !defined(NDEBUG)
    if (blackboard) {
        std::unordered_map<const rhi::ITexture*, DeclaredOpsRecord> textureOps;
        int warnBudget = 32;
        for (size_t i = 0; i < allPasses.size(); ++i) {
            IFrameGraphPass* pass = allPasses[i];
            if (!pass)
                continue;
            rhi::IRenderTarget* rt = nullptr;
            RenderAttachmentOps ops{};
            if (!pass->DescribeRenderAttachmentOps(*blackboard, rt, ops) || !rt)
                continue;
            const auto checkTexture = [&](rhi::ITexture* tex) {
                if (!tex || warnBudget <= 0)
                    return;
                const auto it = textureOps.find(tex);
                if (it == textureOps.end()) {
                    textureOps.emplace(tex, DeclaredOpsRecord{ops, static_cast<uint32_t>(i), pass->DebugName()});
                    return;
                }
                if (SameOps(it->second.ops, ops))
                    return;
                --warnBudget;
                core::Log::WarnF(
                    "FrameGraph::Compile: attachment-ops contract mismatch for one render target texture between pass %u '%s' and pass %u '%s'.",
                    static_cast<unsigned>(it->second.passIndex), it->second.passName ? it->second.passName : "(null)",
                    static_cast<unsigned>(i), pass->DebugName() ? pass->DebugName() : "(null)");
            };
            checkTexture(rt->GetColorTexture(0));
            if (ops.hasDepth)
                checkTexture(rt->GetDepthTexture());
        }
    }
    {
        int warnBudget = 32;
        for (size_t i = 0; i < allPasses.size(); ++i)
            WarnIntrPassBarrierConflicts(allPasses[i], static_cast<uint32_t>(i), m_barrierPlans[i], warnBudget);
    }
#endif
    MergeBarrierPlansAgainstTracker(device);
#if !defined(NDEBUG)
    ValidateMergedBarrierPlanDebug(device, m_barrierPlans);
    {
        int warnBudget = 32;
        WarnFirstMergedBarrierTextureUnknownLayout(device, m_barrierPlans, warnBudget);
        WarnInterPassSimulatedLayoutGap(device, m_barrierPlans, warnBudget);
    }
    if (!m_passes.empty()) {
        core::Log::DebugF("FrameGraph::Compile: %u passes", static_cast<unsigned>(m_passes.size()));
    }
#endif
}

void FrameGraph::Execute(rhi::IDevice* device, rhi::ICommandList* cmd, const FrameGraphBlackboard* blackboard) {
    (void)blackboard;
    if (!cmd)
        return;

    // Build unified ordered pass view matching Compile order.
    std::vector<IFrameGraphPass*> allPasses;
    allPasses.reserve(m_passes.size() + m_borrowedPasses.size());
    for (auto& p : m_passes) if (p) allPasses.push_back(p.get());
    for (auto* p : m_borrowedPasses) if (p) allPasses.push_back(p);

    assert(m_barrierPlans.size() == allPasses.size() && "FrameGraph::Execute without matching Compile barrier plans");

    for (size_t i = 0; i < allPasses.size(); ++i) {
        IFrameGraphPass* p = allPasses[i];
        if (!p)
            continue;

        const FrameGraphBarrierList& barriers = m_barrierPlans[i];

        if (device && device->GetBarrierTracker()) {
            for (const auto& e : barriers.entries) {
                if (e.texture)
                    device->TransitionTextureToState(e.texture, e.toState);
            }
            device->FlushBarrierTracker(cmd);
        }

        const char* passName = p->DebugName();
        GpuProfiler::Scope passScope(cmd, passName && passName[0] ? passName : "FrameGraphPass");
        p->Execute(cmd);
    }
}

}  // namespace rendering
}  // namespace eyelib

