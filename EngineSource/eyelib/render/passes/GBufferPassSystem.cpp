#include "eyelib/render/passes/GBufferPassSystem.h"
#include "eyelib/render/passes/GBufferPass.h"
#include "eyelib/render/passes/GpuCullPass.h"
#include "eyelib/render/renderer/GBuffer.h"
#include "eyelib/render/resources/Mesh.h"
#include "eyelib/render/RenderContext.h"
#include "eyelib/render/submission/DrawBatch.h"
#include "eyelib/debug/Profiler.h"
#include <cstring>
#include <vector>

namespace eyelib {
namespace render {

void GBufferPassSystem::RenderOpaqueGeometry(const GBufferPassOpaqueParams& p) const {
    if (!p.gbufferPass || !p.gbuffer) return;
    RenderContext ctx{};
    ctx.device = p.device;
    ctx.cmd = p.cmd;
    if (!ctx) return;
    if (p.ensureCurrentFrameResourceSets) p.ensureCurrentFrameResourceSets();
    if (p.frameUbo) {
        math::Mat4 viewProj = p.cam.projection * p.cam.view;
        float frameUbo[5 * 16];
        std::memcpy(frameUbo, &p.cam.view[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 16, &p.cam.projection[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 32, &viewProj[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 48, &p.prevView[0][0], 16 * sizeof(float));
        std::memcpy(frameUbo + 64, &p.prevProjection[0][0], 16 * sizeof(float));
        p.frameUbo->Update(frameUbo, 0, 320);
    }
    const bool useIndirectQuad = p.useGpuDrivenGBuffer && p.gpuCullPass && p.gpuCullPass->IsSupported(p.device)
        && p.gpuDrivenQuad && p.gpuDrivenQuad->IsValid() && (p.gpuCullDrawListCount == 0);
    const bool useIndirectScene = p.useGpuDrivenGBuffer && p.gpuCullPass && p.gpuCullPass->IsSupported(p.device)
        && p.gpuCullDrawListCount > 0 && p.gpuCullSingleMesh && p.gpuCullRepresentativeMesh && p.gpuCullRepresentativeMesh->IsValid()
        && p.gpuCullTransformBuffer;
    if (useIndirectQuad) {
        p.gbufferPass->RenderIndirect(*p.gbuffer, ctx,
            p.gpuCullPass->GetIndirectBuffer(),
            p.gpuCullPass->GetDrawCount(),
            p.gpuCullPass->GetIndirectStride(),
            p.gpuDrivenQuad,
            p.cam.view, p.cam.projection, &p.prevView, &p.prevProjection);
        debug::Profiler::AddFrameCounter("drawCalls", 1);
        debug::Profiler::AddFrameCounter("triangles", 2);
        debug::Profiler::AddFrameCounter("stateChanges", 1);
    } else if (useIndirectScene) {
        p.gbufferPass->RenderIndirectWithPerDrawData(*p.gbuffer, ctx,
            p.gpuCullPass->GetIndirectBuffer(),
            p.gpuCullPass->GetDrawCount(),
            p.gpuCullPass->GetIndirectStride(),
            p.gpuCullRepresentativeMesh.get(),
            p.gpuCullTransformBuffer,
            p.gpuCullDrawListCount,
            p.cam.view, p.cam.projection, &p.prevView, &p.prevProjection);
        debug::Profiler::AddFrameCounter("drawCalls", p.gpuCullDrawListCount);
        const uint32_t idxCount = GetIndexCountForDraw(p.gpuCullRepresentativeMesh.get(), -1);
        debug::Profiler::AddFrameCounter("triangles", (idxCount / 3u) * p.gpuCullDrawListCount);
        debug::Profiler::AddFrameCounter("stateChanges", 1);
    } else {
        if (!p.frameData) return;
        std::vector<DrawBatch> batches = BuildBatches(p.frameData->visibleOpaque);
        std::vector<Drawable> singles;
        for (DrawBatch& batch : batches) {
            if (batch.IsInstanced() && p.instanceMatrixBuffer && batch.transforms.size() <= kMaxInstanceBatchSize) {
                const size_t byteSize = batch.transforms.size() * sizeof(math::Mat4);
                p.instanceMatrixBuffer->Update(batch.transforms.data(), 0, static_cast<uint32_t>(byteSize));
                p.gbufferPass->RenderInstanced(*p.gbuffer, p.device, p.cmd,
                    batch.mesh.get(), batch.material.get(), batch.materialInstance.get(), p.instanceMatrixBuffer,
                    static_cast<uint32_t>(batch.transforms.size()),
                    p.cam.view, p.cam.projection, p.prevView, p.prevProjection, batch.submeshIndex);
                debug::Profiler::AddFrameCounter("drawCalls", 1);
                debug::Profiler::AddFrameCounter("triangles", (GetIndexCountForDraw(batch.mesh.get(), batch.submeshIndex) / 3u) * static_cast<uint32_t>(batch.transforms.size()));
                debug::Profiler::AddFrameCounter("stateChanges", 1);
            } else {
                for (size_t i = 0; i < batch.transforms.size(); ++i)
                    singles.push_back(batch.ToDrawable(i));
            }
        }
        if (!singles.empty()) {
            for (const Drawable& d : singles) {
                debug::Profiler::AddFrameCounter("drawCalls", 1);
                debug::Profiler::AddFrameCounter("triangles", GetIndexCountForDraw(d.mesh.get(), d.submeshIndex) / 3u);
                debug::Profiler::AddFrameCounter("stateChanges", 1);
            }
            p.gbufferPass->Render(*p.gbuffer, ctx, singles, p.cam.view, p.cam.projection, p.prevView, p.prevProjection);
        }
    }
}

}  // namespace render
}  // namespace eyelib
