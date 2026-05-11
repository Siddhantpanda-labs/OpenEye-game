#include "eyelib/rendering/features/editor/DebugDrawPass.h"

#include "eyelib/rendering/core/FrameViewUniforms.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraphBarrierList.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/rhi/RHITypes.h"

#include <cstring>

namespace eyelib {
namespace rendering {

namespace {

struct DebugLineVertex {
    float pos[3];
    float color[4];
};
static_assert(sizeof(DebugLineVertex) == 28, "DebugLineVertex must match shader vertex layout (vec3 + vec4)");

}  // namespace

DebugDrawPass::DebugDrawPass(FrameGraphBlackboard* blackboard, rhi::IDevice* device, rhi::IPipeline* pipeline) noexcept
    : m_blackboard(blackboard), m_device(device), m_pipeline(pipeline) {}

DebugDrawPass::~DebugDrawPass() {
    DestroyDeviceResources();
}

void DebugDrawPass::Reconfigure(FrameGraphBlackboard* blackboard, rhi::IPipeline* pipeline) noexcept {
    m_blackboard = blackboard;
    m_pipeline = pipeline;
}

void DebugDrawPass::SetLines(const DebugLine* lines, size_t count) noexcept {
    m_lines = lines;
    m_lineCount = count;
}

void DebugDrawPass::DestroyDeviceResources() noexcept {
    if (!m_device) return;
    for (uint32_t i = 0; i < kFrameSlots; ++i) {
        if (m_vertexBuffers[i]) {
            m_device->DestroyBuffer(m_vertexBuffers[i]);
            m_vertexBuffers[i] = nullptr;
            m_vertexBufferCapacityBytes[i] = 0;
        }
        if (m_frameSets[i]) {
            m_device->DestroyDescriptorSet(m_frameSets[i]);
            m_frameSets[i] = nullptr;
            m_frameSetBuffersKeys[i] = 0;
        }
    }
}

void DebugDrawPass::EnsureVertexBufferCapacity(uint32_t frameIdx, size_t requiredBytes) {
    if (frameIdx >= kFrameSlots) return;
    if (m_vertexBufferCapacityBytes[frameIdx] >= requiredBytes && m_vertexBuffers[frameIdx])
        return;

    if (m_vertexBuffers[frameIdx]) {
        m_device->DestroyBuffer(m_vertexBuffers[frameIdx]);
        m_vertexBuffers[frameIdx] = nullptr;
    }

    // Geometric growth to keep amortised allocation cost down across grow events.
    size_t newCapacity = m_vertexBufferCapacityBytes[frameIdx] > 0 ? m_vertexBufferCapacityBytes[frameIdx] * 2 : 4096u;
    while (newCapacity < requiredBytes) newCapacity *= 2;

    rhi::BufferDesc desc{};
    desc.sizeInBytes = newCapacity;
    desc.usage = rhi::BufferUsage::Vertex;
    desc.memoryHint = rhi::BufferMemoryHint::HostVisible;
    desc.debugName = "EditorDebugLines.Vertex";
    m_vertexBuffers[frameIdx] = m_device->CreateBuffer(desc, nullptr);
    m_vertexBufferCapacityBytes[frameIdx] = m_vertexBuffers[frameIdx] ? newCapacity : 0;
}

void DebugDrawPass::CollectTextureBarriers(const FrameGraphBlackboard& blackboard, FrameGraphBarrierList& out) {
    rhi::IRenderTarget* rt = blackboard.TryGetRenderTarget(BlackboardSlot::MainHdrRenderTarget);
    if (!rt) return;
    if (rhi::ITexture* color = rt->GetColorTexture(0))
        out.Push(color, rhi::ResourceState::RenderTarget);
    if (rhi::ITexture* depth = rt->GetDepthTexture())
        out.Push(depth, rhi::ResourceState::DepthRead);
}

bool DebugDrawPass::DescribeRenderAttachmentOps(const FrameGraphBlackboard& blackboard, rhi::IRenderTarget*& outRt,
                                                RenderAttachmentOps& outOps) const {
    outRt = blackboard.TryGetRenderTarget(BlackboardSlot::MainHdrRenderTarget);
    if (!outRt) return false;

    // Overlay on top of existing HDR contents — never clear here.
    outOps.color.loadOp = AttachmentLoadOp::Load;
    outOps.color.storeOp = AttachmentStoreOp::Store;

    // Keep depth read-only so lines occlude correctly against opaque geometry without disturbing the
    // depth buffer for any subsequent passes that may want to read it.
    outOps.hasDepth = outRt->GetDepthTexture() != nullptr;
    outOps.depth.loadOp = AttachmentLoadOp::Load;
    outOps.depth.storeOp = AttachmentStoreOp::Store;
    return true;
}

void DebugDrawPass::Execute(rhi::ICommandList* cmd) {
    if (!cmd || !m_blackboard || !m_pipeline || !m_device || !m_lines || m_lineCount == 0)
        return;

    rhi::IRenderTarget* rt = m_blackboard->TryGetRenderTarget(BlackboardSlot::MainHdrRenderTarget);
    if (!rt) return;

    rhi::IBuffer* perFrame = m_blackboard->TryGetBuffer(BlackboardSlot::PerFrameUniformBuffer);
    rhi::IBuffer* perView  = m_blackboard->TryGetBuffer(BlackboardSlot::PerViewUniformBuffer);
    if (!perFrame || !perView) return;

    const uint32_t frameIdx = m_device->GetCurrentFrameIndex();
    if (frameIdx >= kFrameSlots) return;

    const uint32_t vertexCount = static_cast<uint32_t>(m_lineCount) * 2u;
    const size_t   requiredBytes = vertexCount * sizeof(DebugLineVertex);
    EnsureVertexBufferCapacity(frameIdx, requiredBytes);
    rhi::IBuffer* vb = m_vertexBuffers[frameIdx];
    if (!vb) return;

    // Host-visible vertex buffer is mapped + filled each frame; persistent map is not required for
    // an overlay this small.
    void* mapped = vb->Map(0, requiredBytes);
    if (!mapped) return;
    DebugLineVertex* dst = static_cast<DebugLineVertex*>(mapped);
    for (size_t i = 0; i < m_lineCount; ++i) {
        const DebugLine& L = m_lines[i];
        dst[i * 2 + 0].pos[0] = L.a.x; dst[i * 2 + 0].pos[1] = L.a.y; dst[i * 2 + 0].pos[2] = L.a.z;
        dst[i * 2 + 0].color[0] = L.color.x; dst[i * 2 + 0].color[1] = L.color.y;
        dst[i * 2 + 0].color[2] = L.color.z; dst[i * 2 + 0].color[3] = L.color.w;
        dst[i * 2 + 1].pos[0] = L.b.x; dst[i * 2 + 1].pos[1] = L.b.y; dst[i * 2 + 1].pos[2] = L.b.z;
        dst[i * 2 + 1].color[0] = L.color.x; dst[i * 2 + 1].color[1] = L.color.y;
        dst[i * 2 + 1].color[2] = L.color.z; dst[i * 2 + 1].color[3] = L.color.w;
    }
    vb->Unmap();

    const uint64_t frameKey = (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perFrame)) << 32) ^
                              static_cast<uint64_t>(reinterpret_cast<uintptr_t>(perView));
    if (!m_frameSets[frameIdx] || frameKey != m_frameSetBuffersKeys[frameIdx]) {
        if (!m_frameSets[frameIdx]) {
            rhi::IPipelineLayout* frameLayout = m_device->GetResourceSetLayout(rhi::IDevice::ResourceSetType::Frame);
            if (frameLayout) m_frameSets[frameIdx] = m_device->CreateDescriptorSet(frameLayout);
        }
        m_frameSetBuffersKeys[frameIdx] = frameKey;
        if (m_frameSets[frameIdx]) {
            m_frameSets[frameIdx]->UpdateUniformBuffer(0, perFrame, 0, kPerFrameUniformGpuBytes);
            m_frameSets[frameIdx]->UpdateUniformBuffer(1, perView, 0, kPerViewUniformGpuBytes);
        }
    }
    if (!m_frameSets[frameIdx]) return;

    RenderAttachmentOps ops{};
    rhi::IRenderTarget* unused = nullptr;
    if (!DescribeRenderAttachmentOps(*m_blackboard, unused, ops)) return;

    cmd->SetRenderTarget(rt, ops);
    const uint32_t vw = rt->GetWidth();
    const uint32_t vh = rt->GetHeight();
    if (vw > 0 && vh > 0) cmd->SetViewport(0, 0, vw, vh);

    cmd->SetDepthTest(true);
    cmd->SetDepthWrite(false);
    cmd->SetDepthFuncLess(false);  // LessEqual via shader pipeline; runtime state stays neutral.
    cmd->SetCullFace(false);

    cmd->BindPipeline(m_pipeline);
    cmd->BindVertexBuffer(vb, 0, 0);
    cmd->BindDescriptorSets(0, 1, &m_frameSets[frameIdx]);
    cmd->Draw(vertexCount, 0);
}

}  // namespace rendering
}  // namespace eyelib
