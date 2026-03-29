#include "eyelib/render/passes/ShadowPassSystem.h"
#include "eyelib/render/shadows/CascadeShadowMap.h"
#include "eyelib/render/shadows/ShadowPass.h"
#include "eyelib/render/submission/DrawBatch.h"
#include "eyelib/debug/Profiler.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace eyelib {
namespace render {

void ShadowPassSystem::RenderShadowBatched(const ShadowPassBatchedParams& p) const {
    if (!p.shadowPass || !p.frameData || !p.frameData->light.castShadows) return;
    const float aspect = (p.camView.projection[0][0] != 0.0f)
        ? std::abs(p.camView.projection[1][1] / p.camView.projection[0][0])
        : 1.0f;
    const float shadowFar = std::min(p.camView.farPlane, p.maxDirectionalShadowDistance);
    std::vector<DrawBatch> batches = BuildBatches(p.frameData->allOpaque);
    std::vector<Drawable> singles;
    for (DrawBatch& batch : batches) {
        if (batch.IsInstanced() && p.instanceMatrixBuffer && batch.transforms.size() <= kMaxInstanceBatchSize) {
            const size_t byteSize = batch.transforms.size() * sizeof(math::Mat4);
            p.instanceMatrixBuffer->Update(batch.transforms.data(), 0, static_cast<uint32_t>(byteSize));
            p.shadowPass->RenderInstanced(*p.cascadeShadowMap, p.device, p.cmd,
                batch.mesh.get(), batch.material.get(), p.instanceMatrixBuffer,
                static_cast<uint32_t>(batch.transforms.size()),
                p.frameData->light.direction, p.camView.view, p.camView.projection, p.camView.viewPos,
                p.camView.nearPlane, shadowFar, aspect, batch.submeshIndex);
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
        p.shadowPass->Render(*p.cascadeShadowMap, p.device, p.cmd,
            [&singles](std::function<void(const Drawable&)> cb) { for (const auto& d : singles) cb(d); },
            p.frameData->light.direction, p.camView.view, p.camView.projection, p.camView.viewPos, p.camView.nearPlane, shadowFar, aspect);
    }
}

}  // namespace render
}  // namespace eyelib
