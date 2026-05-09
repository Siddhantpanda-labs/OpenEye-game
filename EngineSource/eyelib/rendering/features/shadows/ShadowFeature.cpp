#include "eyelib/rendering/features/shadows/ShadowFeature.h"
#include "eyelib/core/Log.h"
#include "eyelib/rendering/core/DirectionalLightUniforms.h"
#include "eyelib/rendering/core/SpotLightUniforms.h"
#include "eyelib/rendering/features/shadows/DirectionalShadowPass.h"
#include "eyelib/rendering/features/shadows/SpotShadowPass.h"
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
#include "eyelib/rendering/features/shadows/ShadowBlurPass.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>


namespace eyelib {
namespace rendering {

namespace {

struct ShadowParamsStd140 {
  float splitDepths[4] = {10.0f, 30.0f, 80.0f, 0.0f};
  float biasAndFilter[4] = {
      0.0001f, 0.05f, 0.0f,
      0.08f}; // depthBias, normalBias, reserved, cascadeBlend
  float filterExtent[4] = {
      1.0f, 2.0f, 3.0f, 0.0f}; // Extent for cascade 0, 1, 2
  float lightViewProj[3][16] = {
      {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f,
       0.f, 1.f},
      {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f,
       0.f, 1.f},
      {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f,
       0.f, 1.f},
  };
  float padding[12] = {};
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
  // Light view: eye at WORLD ORIGIN (zero), look along light direction.
  // Wicked Engine comment: "important to not move (zero out eye vector) the light view matrix
  // itself because texel snapping must be done on projection matrix!"
  // Moving the eye causes the snap math to fight the view translation every frame → swimming.
  const math::Vec3 worldUp(0.0f, 1.0f, 0.0f);
  const float upDot = glm::abs(glm::dot(lightDir, worldUp));
  const math::Vec3 safeUp = (upDot > 0.99f) ? math::Vec3(1.0f, 0.0f, 0.0f) : worldUp;
  // lightView maps world → light view space; eye is at origin.
  const math::Mat4 lightView = glm::lookAt(math::Vec3(0.0f), lightDir, safeUp);

  constexpr float kShadowMapResolution = 2048.0f;
  for (int c = 0; c < cascadeCount; ++c) {
    const float nearDist = cascadeEnds[c];
    const float farDist  = cascadeEnds[c + 1];
    const float tNear    = (nearDist - n) / clipRange;
    const float tFar     = (farDist  - n) / clipRange;

    // Transform frustum corners into LIGHT-VIEW-SPACE (not world space).
    // Working in light space avoids world-space floating-point precision loss
    // and makes the bounding sphere radius rotation-invariant exactly.
    math::Vec3 corners[8];
    for (int i = 0; i < 4; ++i) {
      const math::Vec3 wNear = frustumNear[i] + (frustumFar[i] - frustumNear[i]) * tNear;
      const math::Vec3 wFar  = frustumNear[i] + (frustumFar[i] - frustumNear[i]) * tFar;
      corners[i]     = math::Vec3(lightView * math::Vec4(wNear, 1.0f));
      corners[i + 4] = math::Vec3(lightView * math::Vec4(wFar,  1.0f));
    }

    // Bounding sphere in light-view-space.
    math::Vec3 lsCenter(0.0f);
    for (const math::Vec3& v : corners) lsCenter += v;
    lsCenter *= (1.0f / 8.0f);

    float radius = 0.0f;
    for (const math::Vec3& v : corners)
      radius = (std::max)(radius, glm::length(v - lsCenter));

    // Fit AABB onto bounding sphere in light space.
    math::Vec3 vMin = lsCenter - math::Vec3(radius);
    math::Vec3 vMax = lsCenter + math::Vec3(radius);

    // Texel-snap in light-view-space (Wicked Engine technique):
    // Snap vMin and vMax to the shadow-map texel grid BEFORE building the ortho matrix.
    // This ensures the projection bounds only move in whole-texel steps regardless of
    // camera rotation or translation → zero sub-texel drift → stable shadow edges.
    const float texelSizeX = (vMax.x - vMin.x) / kShadowMapResolution;
    const float texelSizeY = (vMax.y - vMin.y) / kShadowMapResolution;
    vMin.x = glm::floor(vMin.x / texelSizeX) * texelSizeX;
    vMin.y = glm::floor(vMin.y / texelSizeY) * texelSizeY;
    vMax.x = glm::floor(vMax.x / texelSizeX) * texelSizeX;
    vMax.y = glm::floor(vMax.y / texelSizeY) * texelSizeY;
    // Recompute center from snapped bounds.
    const math::Vec3 center = (vMin + vMax) * 0.5f;

    // Z range: tight × 4 for depth precision (Wicked Engine: ext * 4).
    // This allows casters behind the camera frustum without ballooning the depth range.
    const float zExt = glm::abs(center.z - vMin.z) * 4.0f;
    const float zNearLS = center.z - zExt;
    const float zFarLS  = center.z + zExt;

    // Build ortho in light-view-space directly from snapped AABB.
    // GLM ortho: near/far along +Z axis in view space.
    // lightView looks in +Z (light direction), so near = zNearLS, far = zFarLS.
    math::Mat4 lightProj = glm::ortho(vMin.x, vMax.x, vMin.y, vMax.y, zNearLS, zFarLS);

    // Apply Vulkan [0,1] depth range + Y-flip.
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
  (void)registry.TryReserve(BlackboardSlot::SpotLightUniformBuffer,
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
  (void)registry.TryRelease(BlackboardSlot::SpotLightUniformBuffer,
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
    std::vector<rhi::RHITextureFormat> colorFormats = {rhi::RHITextureFormat::R16G16_SFLOAT};
    m_directionalShadowRt =
        render.device->CreateRenderTargetArray(2048u, 2048u, 3u, colorFormats, rhi::RHITextureFormat::Depth24);
  }
  if (!m_shadowPipeline) {
    rhi::ShaderGraphicsPipelineCreateInfo gp{};
    gp.assetPath = "raster/shadow/directional_vsm";
    gp.vertexLayout = Mesh::GetVertexLayoutRHI();
    gp.topology = rhi::PrimitiveTopology::TriangleList;
    gp.layoutKind = rhi::PipelineLayoutKind::PushOnly;
    gp.sourcePolicy = rhi::ShaderSourcePolicy::PreferSpirv;
    gp.useDepthStencilOverride = true;
    gp.depthStencilOverride.depthTest = true;
    gp.depthStencilOverride.depthWrite = true;
    gp.depthStencilOverride.depthCompare = rhi::DepthCompare::Less;
    rhi::RasterState rasterState{};
    rasterState.depthBiasEnable = true;   // Required for vkCmdSetDepthBias to take effect (see P7.3a)
    rasterState.depthBiasConstant = 0.f;  // Overridden per-frame in DirectionalShadowPass::Execute
    rasterState.depthBiasSlope = 0.f;
    gp.rasterStateOverride = rasterState;
    rhi::PipelineAttachmentFormats att{};
    att.color = {rhi::RHITextureFormat::R16G16_SFLOAT};
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

  // Spot light logic
  uint32_t spotCount = 0;
  const SceneLighting::SpotLightSceneState* spotLights = SceneLighting::GetSpotLights(spotCount);
  
  if (!m_spotShadowRt) {
      std::vector<rhi::RHITextureFormat> colorFormats = {rhi::RHITextureFormat::R16G16_SFLOAT};
      m_spotShadowRt = render.device->CreateRenderTargetArray(512u, 512u, std::max(1u, spotCount), colorFormats, rhi::RHITextureFormat::Depth24);
      m_spotShadowScratchRt = render.device->CreateRenderTargetArray(512u, 512u, std::max(1u, spotCount), colorFormats, rhi::RHITextureFormat::Depth24); // Needs depth? No, but let's keep format same
  }
  
  if (!m_spotLightUniformBuffer) {
      rhi::BufferDesc d{};
      d.sizeInBytes = kSpotLightUniformGpuBytes;
      d.usage = rhi::BufferUsage::Uniform;
      d.memoryHint = rhi::BufferMemoryHint::HostVisible;
      d.debugName = "ShadowFeature.SpotLightUniformBuffer";
      m_spotLightUniformBuffer = render.device->CreateBuffer(d, nullptr);
  }
  
  if (m_spotLightUniformBuffer) {
      SpotLightArrayStd140 spotData{};
      spotData.countAndPadding.x = static_cast<float>(spotCount);
      for (uint32_t i = 0; i < std::min(spotCount, kMaxSpotLights); ++i) {
          const auto& sl = spotLights[i];
          spotData.lights[i].positionAndRange = math::Vec4(sl.position, sl.range);
          spotData.lights[i].directionAndInnerCone = math::Vec4(sl.direction, sl.innerConeCos);
          spotData.lights[i].colorAndOuterCone = math::Vec4(sl.color * sl.intensity, sl.outerConeCos);
          
          math::Mat4 proj = glm::perspective(std::acos(sl.outerConeCos) * 2.0f, 1.0f, 0.1f, sl.range);
          proj = camera::AdjustProjectionForBackend(proj, camera::PolicyForBackend(RenderBackend::Vulkan));
          const math::Vec3 worldUp(0.0f, 1.0f, 0.0f);
          const math::Vec3 safeUp = (glm::abs(glm::dot(sl.direction, worldUp)) > 0.99f)
              ? math::Vec3(1.0f, 0.0f, 0.0f) : worldUp;
          math::Mat4 view = glm::lookAt(sl.position, sl.position + sl.direction, safeUp);
          spotData.lights[i].lightViewProj = proj * view;
          
          spotData.lights[i].shadowParams = math::Vec4(sl.castShadows ? static_cast<float>(i) : -1.0f, 0.0f, 0.0f, 0.0f);
      }
      m_spotLightUniformBuffer->Update(&spotData, 0, sizeof(spotData));
  }
}

void ShadowFeature::OnSchedulePasses(FrameGraph &graph,
                                     FrameGraphBlackboard &blackboard,
                                     const RenderContext &render) {
  (void)render;
  if (m_directionalShadowRt) {
    if (rhi::ITexture *color = m_directionalShadowRt->GetColorTexture(0)) {
      blackboard.PublishTexture(BlackboardSlot::DirectionalShadowMapArray,
                                writer_names::kShadowFeature, color);
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

  if (m_spotShadowRt) {
      if (rhi::ITexture* color = m_spotShadowRt->GetColorTexture(0)) {
          blackboard.PublishTexture(BlackboardSlot::SpotShadowMapAtlas, writer_names::kShadowFeature, color);
      }
  }
  if (m_spotLightUniformBuffer) {
      blackboard.PublishBuffer(BlackboardSlot::SpotLightUniformBuffer, writer_names::kShadowFeature, m_spotLightUniformBuffer);
  }

  if (m_spotShadowRt && m_shadowPipeline && m_meshResolver && m_shadowCasterList) {
      uint32_t spotCount = 0;
      const SceneLighting::SpotLightSceneState* spotLights = SceneLighting::GetSpotLights(spotCount);
      std::vector<math::Mat4> spotProjs;
      for (uint32_t i = 0; i < std::min(spotCount, kMaxSpotLights); ++i) {
          if (spotLights[i].castShadows) {
              math::Mat4 proj = glm::perspective(std::acos(spotLights[i].outerConeCos) * 2.0f, 1.0f, 0.1f, spotLights[i].range);
              proj = camera::AdjustProjectionForBackend(proj, camera::PolicyForBackend(RenderBackend::Vulkan));
              const math::Vec3 worldUp(0.0f, 1.0f, 0.0f);
              const math::Vec3 safeUp = (glm::abs(glm::dot(spotLights[i].direction, worldUp)) > 0.99f)
                  ? math::Vec3(1.0f, 0.0f, 0.0f) : worldUp;
              math::Mat4 view = glm::lookAt(spotLights[i].position, spotLights[i].position + spotLights[i].direction, safeUp);
              spotProjs.push_back(proj * view);
              core::Log::DebugF("ShadowFeature: scheduling spot shadow pass for light %u (castShadows=%d, range=%.1f)",
                  i, spotLights[i].castShadows, spotLights[i].range);
          }
      }
      
      if (spotCount > 0) {
          if (!m_spotShadowPass) m_spotShadowPass = std::make_unique<SpotShadowPass>();
          
          m_spotShadowPass->Setup(m_spotShadowRt, m_shadowPipeline, m_meshResolver, m_shadowCasterList, spotProjs);
          graph.AddPassBorrowed(m_spotShadowPass.get());
          
          blackboard.PublishTexture(BlackboardSlot::SpotShadowMapAtlas, writer_names::kShadowFeature,
                                    m_spotShadowRt->GetColorTexture(0));
      }
  }
}

void ShadowFeature::OnShutdown() {
  m_directionalShadowPass.reset();
  m_spotShadowPass.reset();
  m_shadowBlurPass.reset();
  if (m_device) {
    if (m_shadowPipeline)
      m_device->DestroyPipeline(m_shadowPipeline);
    if (m_shadowParamsBuffer)
      m_device->DestroyBuffer(m_shadowParamsBuffer);
    if (m_spotLightUniformBuffer) {
        m_device->DestroyBuffer(m_spotLightUniformBuffer);
        m_spotLightUniformBuffer = nullptr;
    }
    if (m_directionalShadowRt)
      m_device->DestroyRenderTarget(m_directionalShadowRt);
    if (m_spotShadowRt) {
      m_device->DestroyRenderTarget(m_spotShadowRt);
      m_spotShadowRt = nullptr;
    }
    if (m_spotShadowScratchRt) {
      m_device->DestroyRenderTarget(m_spotShadowScratchRt);
      m_spotShadowScratchRt = nullptr;
    }
  }
  m_shadowPipeline = nullptr;
  m_shadowParamsBuffer = nullptr;
  m_directionalShadowRt = nullptr;
  m_shadowCasterList = nullptr;
  m_ownedShadowCasterList.reset();
  m_spotShadowCasterList = nullptr;
  m_ownedSpotShadowCasterList.reset();
  m_device = nullptr;
}

} // namespace rendering
} // namespace eyelib
