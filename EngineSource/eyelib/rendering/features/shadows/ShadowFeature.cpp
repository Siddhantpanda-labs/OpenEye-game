#include "eyelib/rendering/features/shadows/ShadowFeature.h"
#include "eyelib/core/Log.h"
#include "eyelib/rendering/core/DirectionalLightUniforms.h"
#include "eyelib/rendering/features/shadows/DirectionalShadowPass.h"
#include "eyelib/rendering/framegraph/BlackboardSlot.h"
#include "eyelib/rendering/framegraph/FrameGraph.h"
#include "eyelib/rendering/framegraph/FrameGraphBlackboard.h"
#include "eyelib/rendering/lighting/SceneLighting.h"
#include "eyelib/rendering/resources/Mesh.h"
#include "eyelib/rendering/rhi/RHI.h"
#include "eyelib/rendering/scene/CameraConventions.h"
#include "eyelib/rendering/scene/CameraView.h"
#include "eyelib/rendering/scene/IRenderSceneView.h"
#include "eyelib/rendering/scene/SceneDrawInstance.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>


namespace eyelib {
namespace rendering {

namespace {

struct ShadowParamsStd140 {
  float splitDepths[4] = {10.0f, 30.0f, 80.0f, 0.0f};
  float biasAndFilter[4] = {
      0.0001f, 0.0005f, 3.0f,
      0.08f}; // depthBias, normalBias, pcfKernel, cascadeBlend
  float lightViewProj[3][16] = {
      {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f,
       0.f, 1.f},
      {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f,
       0.f, 1.f},
      {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f,
       0.f, 1.f},
  };
  float padding[16] = {};
};

static_assert(sizeof(ShadowParamsStd140) <= 512u,
              "Shadow params foundation buffer should stay small");

math::Vec3 ndcToWorld(const math::Mat4 &invViewProj, float x, float y,
                      float z) {
  const math::Vec4 p = invViewProj * math::Vec4(x, y, z, 1.0f);
  return math::Vec3(p) / p.w;
}

void buildDirectionalCascades(const CameraView &cam,
                              const math::Vec3 &lightDirInput,
                              std::array<math::Mat4, 3> &outViewProj,
                              float outSplits[4]) {
  const float n = (cam.nearPlane > 0.0f) ? cam.nearPlane : 0.1f;
  const float f = (cam.farPlane > (n + 0.1f)) ? cam.farPlane : (n + 100.0f);
  const float lambda = 0.7f;
  const int cascadeCount = 3;
  const float clipRange = f - n;

  float cascadeEnds[4] = {n, 0.0f, 0.0f, f};
  for (int i = 1; i < cascadeCount; ++i) {
    const float p = static_cast<float>(i) / static_cast<float>(cascadeCount);
    const float logSplit = n * std::pow(f / n, p);
    const float uniSplit = n + clipRange * p;
    cascadeEnds[i] = lambda * logSplit + (1.0f - lambda) * uniSplit;
  }

  outSplits[0] = cascadeEnds[1];
  outSplits[1] = cascadeEnds[2];
  outSplits[2] = cascadeEnds[3];
  outSplits[3] = 0.0f;

  const math::Mat4 invViewProj = glm::inverse(cam.projection * cam.view);
  const math::Vec3 frustumNear[4] = {
      ndcToWorld(invViewProj, -1.0f, -1.0f, 0.0f),
      ndcToWorld(invViewProj, 1.0f, -1.0f, 0.0f),
      ndcToWorld(invViewProj, 1.0f, 1.0f, 0.0f),
      ndcToWorld(invViewProj, -1.0f, 1.0f, 0.0f),
  };
  const math::Vec3 frustumFar[4] = {
      ndcToWorld(invViewProj, -1.0f, -1.0f, 1.0f),
      ndcToWorld(invViewProj, 1.0f, -1.0f, 1.0f),
      ndcToWorld(invViewProj, 1.0f, 1.0f, 1.0f),
      ndcToWorld(invViewProj, -1.0f, 1.0f, 1.0f),
  };

  const math::Vec3 lightDir = glm::normalize(lightDirInput);
  constexpr float kShadowMapResolution = 2048.0f;
  for (int c = 0; c < cascadeCount; ++c) {
    const float nearDist = cascadeEnds[c];
    const float farDist = cascadeEnds[c + 1];
    const float tNear = (nearDist - n) / clipRange;
    const float tFar = (farDist - n) / clipRange;

    math::Vec3 corners[8];
    for (int i = 0; i < 4; ++i) {
      corners[i] = frustumNear[i] + (frustumFar[i] - frustumNear[i]) * tNear;
      corners[i + 4] = frustumNear[i] + (frustumFar[i] - frustumNear[i]) * tFar;
    }

    math::Vec3 center(0.0f);
    for (const math::Vec3 &v : corners)
      center += v;
    center *= (1.0f / 8.0f);

    float radius = 0.0f;
    for (const math::Vec3 &v : corners)
      radius = (std::max)(radius, glm::length(v - center));
    radius = std::ceil(radius * 16.0f) / 16.0f;

    const math::Vec3 eye = center - lightDir * (radius * 2.5f);
    const math::Vec3 worldUp(0.0f, 1.0f, 0.0f);
    // Avoid lookAt degeneracy when light is (near-)parallel to world up (e.g.,
    // straight top-down light).
    const float upDot = glm::abs(glm::dot(lightDir, worldUp));
    const math::Vec3 safeUp =
        (upDot > 0.99f) ? math::Vec3(0.0f, 0.0f, 1.0f) : worldUp;
    const math::Mat4 lightView = glm::lookAt(eye, center, safeUp);
    // Use a Z-range proportional to the cascade radius to capture casters
    // behind the frustum without destroying depth precision (which would cause
    // massive Peter Panning with fixed biases).
    math::Mat4 lightProj = glm::ortho(-radius, radius, -radius, radius,
                                      -radius * 4.0f, radius * 4.0f);

    // CSM stabilization: snap projection to frustum CENTER (not world origin).
    // Using (0,0,0) fails when camera is far from scene origin.
    const math::Mat4 shadowMatrix = lightProj * lightView;
    const math::Vec4 shadowCenter = shadowMatrix * math::Vec4(center, 1.0f);
    const math::Vec2 shadowTexelCenter =
        math::Vec2(shadowCenter) * (kShadowMapResolution * 0.5f);
    const math::Vec2 roundedCenter = glm::round(shadowTexelCenter);
    const math::Vec2 roundOffset =
        (roundedCenter - shadowTexelCenter) * (2.0f / kShadowMapResolution);
    lightProj[3][0] += roundOffset.x;
    lightProj[3][1] += roundOffset.y;

    // Apply Vulkan Z-range adjustment [0, 1] to prevent near-plane clipping
    lightProj = camera::AdjustProjectionForBackend(
        lightProj, camera::PolicyForBackend(RenderBackend::Vulkan));

    outViewProj[c] = lightProj * lightView;
  }
}

} // namespace

ShadowFeature::ShadowFeature() = default;
ShadowFeature::~ShadowFeature() = default;

void ShadowFeature::OnRegisterBlackboardWriters(
    BlackboardWriterRegistry &registry, const RenderCapabilities &caps) {
  (void)caps;
  (void)registry.TryReserve(BlackboardSlot::DirectionalShadowMapArray,
                            writer_names::kShadowFeature);
  (void)registry.TryReserve(BlackboardSlot::SpotShadowMapAtlas,
                            writer_names::kShadowFeature);
  (void)registry.TryReserve(BlackboardSlot::PointShadowCubemapArray,
                            writer_names::kShadowFeature);
  (void)registry.TryReserve(BlackboardSlot::ShadowParamsBuffer,
                            writer_names::kShadowFeature);
}

void ShadowFeature::OnUnregisterBlackboardWriters(
    BlackboardWriterRegistry &registry) {
  (void)registry.TryRelease(BlackboardSlot::DirectionalShadowMapArray,
                            writer_names::kShadowFeature);
  (void)registry.TryRelease(BlackboardSlot::SpotShadowMapAtlas,
                            writer_names::kShadowFeature);
  (void)registry.TryRelease(BlackboardSlot::PointShadowCubemapArray,
                            writer_names::kShadowFeature);
  (void)registry.TryRelease(BlackboardSlot::ShadowParamsBuffer,
                            writer_names::kShadowFeature);
}

void ShadowFeature::OnUpload(FrameContext &frame, const RenderContext &render) {
  (void)frame;
  if (!render.device)
    return;
  m_device = render.device;
  if (!m_ownedShadowCasterList) {
    m_ownedShadowCasterList = std::make_unique<ShadowCasterList>();
    m_shadowCasterList = m_ownedShadowCasterList.get();
  }
  m_shadowCasterList->Clear();
  if (render.sceneView) {
    const size_t n = render.sceneView->SceneDrawInstanceCount();
    const SceneDrawInstance *instances = render.sceneView->SceneDrawInstances();
    if (instances && n > 0) {
      m_shadowCasterList->items.reserve(n);
      for (size_t i = 0; i < n; ++i) {
        const SceneDrawInstance &src = instances[i];
        if (src.layer != DrawLayer::Opaque && src.layer != DrawLayer::AlphaTest)
          continue;
        if (!src.localBounds.IsValid())
          continue;

        ShadowCasterItem item{};
        item.meshHandle = src.meshHandle;
        item.worldMatrix = src.worldTransform;
        item.worldBounds = src.localBounds.Transform(src.worldTransform);
        item.layer = src.layer;
        item.flags = src.flags;
        m_shadowCasterList->items.push_back(item);
      }
    }
  }

  if (!m_directionalShadowRt) {
    m_directionalShadowRt =
        render.device->CreateDepthOnlyRenderTargetArray(2048u, 2048u, 3u);
  }
  if (!m_shadowPipeline) {
    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    gp.assetPath = "raster/shadow/directional_depth";
    gp.vertexLayout = Mesh::GetVertexLayoutRHI();
    gp.topology = rhi::PrimitiveTopology::TriangleList;
    gp.layoutKind = rhi::PipelineLayoutKind::PushOnly;
    gp.sourcePolicy = rhi::ShaderSourcePolicy::PreferSpirv;
    gp.useDepthStencilOverride = true;
    gp.depthStencilOverride.depthTest = true;
    gp.depthStencilOverride.depthWrite = true;
    gp.depthStencilOverride.depthCompare = rhi::DepthCompare::Less;
    rhi::PipelineAttachmentFormats att{};
    att.depthStencil = rhi::RHITextureFormat::Depth24;
    gp.attachmentFormats = att;
    m_shadowPipeline = render.device->CreateGraphicsPipelineFromShaderAsset(gp);
    if (!m_shadowPipeline) {
      core::Log::Warn(
          "ShadowFeature: failed to create directional shadow pipeline.");
    }
  }
  if (!m_shadowParamsBuffer) {
    rhi::BufferDesc d{};
    d.sizeInBytes = sizeof(ShadowParamsStd140);
    d.usage = rhi::BufferUsage::Uniform;
    d.memoryHint = rhi::BufferMemoryHint::HostVisible;
    d.debugName = "ShadowFeature.DirectionalShadowParams";
    m_shadowParamsBuffer = render.device->CreateBuffer(d, nullptr);
  }
  if (m_shadowParamsBuffer) {
    ShadowParamsStd140 params{};
    if (const CameraView *cam =
            render.sceneView ? render.sceneView->TryGetCamera() : nullptr) {
      const DirectionalLightSceneState sceneLight =
          SceneLighting::GetDirectionalLight();
      buildDirectionalCascades(*cam, sceneLight.direction, m_lightViewProj,
                               params.splitDepths);
      for (int i = 0; i < 3; ++i) {
        const float *m = &m_lightViewProj[i][0][0];
        std::copy(m, m + 16, params.lightViewProj[i]);
      }
    }
    m_shadowParamsBuffer->Update(&params, 0, sizeof(params));
  }
}

void ShadowFeature::OnSchedulePasses(FrameGraph &graph,
                                     FrameGraphBlackboard &blackboard,
                                     const RenderContext &render) {
  (void)render;
  if (m_directionalShadowRt) {
    if (rhi::ITexture *depth = m_directionalShadowRt->GetDepthTexture()) {
      blackboard.PublishTexture(BlackboardSlot::DirectionalShadowMapArray,
                                writer_names::kShadowFeature, depth);
    }
  }
  if (m_shadowParamsBuffer) {
    blackboard.PublishBuffer(BlackboardSlot::ShadowParamsBuffer,
                             writer_names::kShadowFeature,
                             m_shadowParamsBuffer);
  }

  if (m_directionalShadowRt && m_shadowPipeline && m_meshResolver &&
      m_shadowCasterList) {
    if (!m_directionalShadowPass)
      m_directionalShadowPass = std::make_unique<DirectionalShadowPass>(
          m_directionalShadowRt, m_shadowPipeline, m_meshResolver,
          m_shadowCasterList, &m_lightViewProj);
    else
      m_directionalShadowPass->Reconfigure(
          m_directionalShadowRt, m_shadowPipeline, m_meshResolver,
          m_shadowCasterList, &m_lightViewProj);
    graph.AddPassBorrowed(m_directionalShadowPass.get());
  }
}

void ShadowFeature::OnShutdown() {
  m_directionalShadowPass.reset();
  if (m_device) {
    if (m_shadowPipeline)
      m_device->DestroyPipeline(m_shadowPipeline);
    if (m_shadowParamsBuffer)
      m_device->DestroyBuffer(m_shadowParamsBuffer);
    if (m_directionalShadowRt)
      m_device->DestroyRenderTarget(m_directionalShadowRt);
  }
  m_shadowPipeline = nullptr;
  m_shadowParamsBuffer = nullptr;
  m_directionalShadowRt = nullptr;
  m_shadowCasterList = nullptr;
  m_ownedShadowCasterList.reset();
  m_device = nullptr;
}

} // namespace rendering
} // namespace eyelib
