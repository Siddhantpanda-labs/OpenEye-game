#include "eyelib/rendering/features/shadows/ShadowFeature.h"
#include "eyelib/core/Log.h"
#include "eyelib/rendering/core/DirectionalLightUniforms.h"
#include "eyelib/rendering/core/PointLightUniforms.h"
#include "eyelib/rendering/core/SpotLightUniforms.h"
#include "eyelib/rendering/features/shadows/DirectionalShadowPass.h"
#include "eyelib/rendering/features/shadows/PointShadowPass.h"
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
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>


namespace eyelib {
namespace rendering {

namespace {

struct ShadowParamsStd140 {
  float splitDepths[4] = {10.0f, 30.0f, 80.0f, 0.0f};
  float biasAndFilter[4] = {
      0.00025f, 0.015f, 0.0f,
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

void buildPointLightFaceViewProjs(const math::Vec3& pos, float range,
                                  math::Mat4 outFaces[kPointLightFaces]) {
  const float zNear = 0.1f;
  const float zFar = (range > zNear + 0.01f) ? range : (zNear + 0.01f);
  math::Mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, zNear, zFar);
  proj = camera::AdjustProjectionForBackend(proj, camera::PolicyForBackend(RenderBackend::Vulkan));
  const math::Vec3 fwd[kPointLightFaces] = {
      {1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
      {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f},
  };
  const math::Vec3 up[kPointLightFaces] = {
      {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
      {0.0f, 0.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
  };
  for (uint32_t f = 0; f < kPointLightFaces; ++f) {
    const math::Mat4 view = glm::lookAt(pos, pos + fwd[f], up[f]);
    outFaces[f] = proj * view;
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
  (void)registry.TryReserve(BlackboardSlot::PointLightUniformBuffer,
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
  (void)registry.TryRelease(BlackboardSlot::PointLightUniformBuffer,
                            writer_names::kShadowFeature);
  (void)registry.TryRelease(BlackboardSlot::ShadowParamsBuffer,
                            writer_names::kShadowFeature);
}

void ShadowFeature::OnUpload(FrameContext &frame, const RenderContext &render) {
  (void)frame;
  if (!render.device)
    return;
  m_device = render.device;
  if (render.recordingCommandList) {
    render.device->EnsureSharedShadowMapArrayDummyForShaderSampling(render.recordingCommandList);
  }
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
      std::sort(m_shadowCasterList->items.begin(), m_shadowCasterList->items.end(),
          [](const ShadowCasterItem& a, const ShadowCasterItem& b) {
              return a.meshHandle < b.meshHandle;
          });
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
    rhi::RasterState rasterState{};
    rasterState.depthBiasEnable = true;   // Required for vkCmdSetDepthBias to take effect (see P7.3a)
    rasterState.depthBiasConstant = 0.f;  // Overridden per-frame in DirectionalShadowPass::Execute
    rasterState.depthBiasSlope = 0.f;
    gp.rasterStateOverride = rasterState;
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

  // Spot light logic
  uint32_t spotCount = 0;
  const SceneLighting::SpotLightSceneState* spotLights = SceneLighting::GetSpotLights(spotCount);
  const uint32_t clampedSpotCount = std::min(spotCount, kMaxSpotLights);

  m_spotShadowViewProjs.clear();
  m_spotShadowViewProjs.reserve(clampedSpotCount);
  uint32_t shadowCastingSpotCount = 0;
  for (uint32_t i = 0; i < clampedSpotCount; ++i) {
      if (spotLights[i].castShadows)
          ++shadowCastingSpotCount;
  }

  if (shadowCastingSpotCount > 0u) {
      const uint32_t requiredLayers = shadowCastingSpotCount;
      if (!m_spotShadowRt || m_spotShadowLayerCount != requiredLayers) {
          if (m_spotShadowRt)
              render.device->DestroyRenderTarget(m_spotShadowRt);
          m_spotShadowRt = nullptr;
          m_spotShadowRt = render.device->CreateDepthOnlyRenderTargetArray(512u, 512u, requiredLayers);
          m_spotShadowLayerCount = requiredLayers;
      }
  } else {
      if (m_spotShadowRt) {
          render.device->DestroyRenderTarget(m_spotShadowRt);
          m_spotShadowRt = nullptr;
      }
      m_spotShadowLayerCount = 0u;
  }

  if (!m_spotShadowPipeline) {
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
      rhi::RasterState rasterState{};
      rasterState.depthBiasEnable = true;
      rasterState.depthBiasConstant = 0.f;
      rasterState.depthBiasSlope = 0.f;
      gp.rasterStateOverride = rasterState;
      rhi::PipelineAttachmentFormats att{};
      att.depthStencil = rhi::RHITextureFormat::Depth24;
      gp.attachmentFormats = att;
      m_spotShadowPipeline = render.device->CreateGraphicsPipelineFromShaderAsset(gp);
      if (!m_spotShadowPipeline) {
          core::Log::Warn("ShadowFeature: failed to create spot shadow depth pipeline.");
      }
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
      spotData.countAndPadding.x = static_cast<float>(clampedSpotCount);
      for (uint32_t i = 0; i < clampedSpotCount; ++i) {
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
          const math::Mat4 viewProj = proj * view;
          spotData.lights[i].lightViewProj = viewProj;

          if (sl.castShadows) {
              const float shadowLayer = static_cast<float>(m_spotShadowViewProjs.size());
              m_spotShadowViewProjs.push_back(viewProj);
              spotData.lights[i].shadowParams = math::Vec4(shadowLayer, 0.0f, 0.0f, 0.0f);
          } else {
              spotData.lights[i].shadowParams = math::Vec4(-1.0f, 0.0f, 0.0f, 0.0f);
          }
      }
      m_spotLightUniformBuffer->Update(&spotData, 0, sizeof(spotData));
  }

  // Point lights + cubemap shadow layers (6 faces per shadow-casting light, capped).
  uint32_t pointCount = 0;
  const SceneLighting::PointLightSceneState* pointLights = SceneLighting::GetPointLights(pointCount);
  const uint32_t clampedPointCount = std::min(pointCount, kMaxPointLights);

  m_pointShadowFaceViewProjs.clear();
  PointLightArrayStd140 pointData{};
  pointData.countAndPadding.x = static_cast<float>(clampedPointCount);

  uint32_t pointShadowSlot = 0;
  for (uint32_t i = 0; i < clampedPointCount; ++i) {
    const auto& pl = pointLights[i];
    PointLightStd140& dst = pointData.lights[i];
    dst.positionAndRange = math::Vec4(pl.position, pl.range);
    dst.colorIntensity = math::Vec4(pl.color.x * pl.intensity, pl.color.y * pl.intensity,
                                    pl.color.z * pl.intensity, pl.intensity);
    if (pl.castShadows && pointShadowSlot < kMaxPointShadowCasters) {
      const float baseLayer = static_cast<float>(pointShadowSlot * kPointLightFaces);
      dst.shadowParams = math::Vec4(baseLayer, 0.0f, 0.0f, 0.0f);
      math::Mat4 faces[kPointLightFaces];
      buildPointLightFaceViewProjs(pl.position, pl.range, faces);
      for (uint32_t f = 0; f < kPointLightFaces; ++f) {
        dst.faceViewProj[f] = faces[f];
        m_pointShadowFaceViewProjs.push_back(faces[f]);
      }
      ++pointShadowSlot;
    } else {
      dst.shadowParams = math::Vec4(-1.0f, 0.0f, 0.0f, 0.0f);
      for (uint32_t f = 0; f < kPointLightFaces; ++f)
        dst.faceViewProj[f] = math::Mat4(1.0f);
    }
  }

  const uint32_t requiredPointLayers = pointShadowSlot * kPointLightFaces;
  if (pointShadowSlot > 0) {
    if (!m_pointShadowRt || m_pointShadowLayerCount != requiredPointLayers) {
      if (m_pointShadowRt)
        render.device->DestroyRenderTarget(m_pointShadowRt);
      m_pointShadowRt = nullptr;
      constexpr uint32_t kPointShadowResolution = 512u;
      m_pointShadowRt = render.device->CreateDepthOnlyRenderTargetArray(
          kPointShadowResolution, kPointShadowResolution, requiredPointLayers);
      m_pointShadowLayerCount = requiredPointLayers;
    }
  } else {
    if (m_pointShadowRt) {
      render.device->DestroyRenderTarget(m_pointShadowRt);
      m_pointShadowRt = nullptr;
    }
    m_pointShadowLayerCount = 0u;
  }

  if (!m_pointLightUniformBuffer) {
    rhi::BufferDesc d{};
    d.sizeInBytes = kPointLightUniformGpuBytes;
    d.usage = rhi::BufferUsage::Uniform;
    d.memoryHint = rhi::BufferMemoryHint::HostVisible;
    d.debugName = "ShadowFeature.PointLightUniformBuffer";
    m_pointLightUniformBuffer = render.device->CreateBuffer(d, nullptr);
  }
  if (m_pointLightUniformBuffer)
    m_pointLightUniformBuffer->Update(&pointData, 0, sizeof(pointData));
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

  if (m_spotLightUniformBuffer) {
      blackboard.PublishBuffer(BlackboardSlot::SpotLightUniformBuffer, writer_names::kShadowFeature, m_spotLightUniformBuffer);
  }

  const bool hasSpotShadowPass =
      m_spotShadowRt && m_spotShadowPipeline && m_meshResolver && m_shadowCasterList && !m_spotShadowViewProjs.empty();

  if (hasSpotShadowPass) {
      if (!m_spotShadowPass) m_spotShadowPass = std::make_unique<SpotShadowPass>();
      m_spotShadowPass->Setup(m_spotShadowRt, m_spotShadowPipeline, m_meshResolver, m_shadowCasterList, m_spotShadowViewProjs);
      graph.AddPassBorrowed(m_spotShadowPass.get());
  }

  {
      rhi::ITexture* spotDepth = nullptr;
      if (hasSpotShadowPass && m_spotShadowRt)
          spotDepth = m_spotShadowRt->GetDepthTexture();
      if (!spotDepth && render.device) {
          if (rhi::IRenderTarget* dummy = render.device->GetSharedShadowMapArrayDummyRenderTarget())
              spotDepth = dummy->GetDepthTexture();
      }
      if (spotDepth) {
          blackboard.PublishTexture(BlackboardSlot::SpotShadowMapAtlas, writer_names::kShadowFeature, spotDepth);
      }
  }

  if (m_pointLightUniformBuffer) {
    blackboard.PublishBuffer(BlackboardSlot::PointLightUniformBuffer, writer_names::kShadowFeature,
                             m_pointLightUniformBuffer);
  }

  if (m_pointShadowRt && m_spotShadowPipeline && m_meshResolver && m_shadowCasterList &&
      !m_pointShadowFaceViewProjs.empty()) {
    if (!m_pointShadowPass)
      m_pointShadowPass = std::make_unique<PointShadowPass>();
    m_pointShadowPass->Setup(m_pointShadowRt, m_spotShadowPipeline, m_meshResolver, m_shadowCasterList,
                             m_pointShadowFaceViewProjs);
    graph.AddPassBorrowed(m_pointShadowPass.get());
  }

  {
    rhi::ITexture* pointDepth = nullptr;
    if (m_pointShadowRt)
      pointDepth = m_pointShadowRt->GetDepthTexture();
    if (!pointDepth && render.device) {
      // Safe: GetSharedShadowMapArrayDummyRenderTarget guarantees a valid, GPU-initialized RT
      // via its own dedicated VkCommandPool — never returns null on a live device.
      rhi::IRenderTarget* dummyRt = render.device->GetSharedShadowMapArrayDummyRenderTarget();
      if (dummyRt)
        pointDepth = dummyRt->GetDepthTexture();
    }
    if (pointDepth) {
      blackboard.PublishTexture(BlackboardSlot::PointShadowCubemapArray, writer_names::kShadowFeature,
                                pointDepth);
    }
  }
}

void ShadowFeature::OnShutdown() {
  m_directionalShadowPass.reset();
  m_spotShadowPass.reset();
  m_pointShadowPass.reset();
  if (m_device) {
    if (m_shadowPipeline)
      m_device->DestroyPipeline(m_shadowPipeline);
    if (m_spotShadowPipeline)
      m_device->DestroyPipeline(m_spotShadowPipeline);
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
    if (m_pointLightUniformBuffer) {
      m_device->DestroyBuffer(m_pointLightUniformBuffer);
      m_pointLightUniformBuffer = nullptr;
    }
    if (m_pointShadowRt) {
      m_device->DestroyRenderTarget(m_pointShadowRt);
      m_pointShadowRt = nullptr;
    }
  }
  m_shadowPipeline = nullptr;
  m_spotShadowPipeline = nullptr;
  m_shadowParamsBuffer = nullptr;
  m_directionalShadowRt = nullptr;
  m_shadowCasterList = nullptr;
  m_ownedShadowCasterList.reset();
  m_spotShadowViewProjs.clear();
  m_spotShadowLayerCount = 0u;
  m_pointShadowFaceViewProjs.clear();
  m_pointShadowLayerCount = 0u;
  m_device = nullptr;
}

} // namespace rendering
} // namespace eyelib
