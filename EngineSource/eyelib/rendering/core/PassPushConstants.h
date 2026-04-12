#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/rendering/rhi/RHI.h"

namespace eyelib {
namespace rendering {
namespace abi {

/**
 * Canonical push-constant offsets for forward-family pipelines.
 * Shaders under passes/forward use these offsets explicitly.
 */
constexpr uint32_t kForwardVsOffset = 0u;
constexpr uint32_t kForwardFsOffset = 128u;
constexpr uint32_t kShadowVsOffset = 0u;
constexpr uint32_t kEntityIdModelOffset = 0u;
constexpr uint32_t kEntityIdViewOffset = 64u;
constexpr uint32_t kEntityIdProjectionOffset = 128u;
constexpr uint32_t kEntityIdFragOffset = 192u;

/** IBL capture vertex push (matches `passes/ibl/*_convolution.vert` / `prefilter.vert`). */
constexpr uint32_t kIblProjectionOffset = 0u;
constexpr uint32_t kIblViewOffset = 64u;
/** Prefilter fragment `u_Roughness` (after two mat4s in the shared push range). */
constexpr uint32_t kIblRoughnessOffset = 128u;

/** Outline post fullscreen pass (`passes/outline_post/outline_post.frag`), std430 packing. */
constexpr uint32_t kOutlinePostSelectedIdOffset = 0u;
constexpr uint32_t kOutlinePostColorOffset = 16u;
constexpr uint32_t kOutlinePostInvSizeOffset = 28u;

/** View + projection only (no model): `debug/lines.vert`, `passes/2d/sprite.vert` — mat4 @0 / @64. */
constexpr uint32_t kViewProjPairViewOffset = 0u;
constexpr uint32_t kViewProjPairProjectionOffset = 64u;

/** Skybox fullscreen passes (`passes/sky/skybox.frag`, `skybox_procedural.frag`) — inverse view @0, inverse projection @64. */
constexpr uint32_t kSkyboxInverseViewOffset = 0u;
constexpr uint32_t kSkyboxInverseProjectionOffset = 64u;
/** `u_SkyboxBrightness` multiplier (std430 after two mat4). */
constexpr uint32_t kSkyboxBrightnessOffset = 128u;

/** G-buffer geometry (`passes/deferred/gbuffer.vert` + `gbuffer.frag`), std430: VS block then FS float. */
constexpr uint32_t kGBufferFragHasAlbedoOffset = 92u;
/** Instanced G-buffer (`gbuffer_instanced.vert` + `.frag`): FS flag after 28-byte VS push. */
constexpr uint32_t kGBufferInstancedFragHasAlbedoOffset = 28u;
/** `gbuffer_skinned.vert`: `u_UsePerDrawTransformBuffer` after VS+FS shared region (92 + 4). */
constexpr uint32_t kGBufferSkinnedUsePerDrawTransformOffset = 96u;

struct ForwardVsPush {
    math::Mat4 model;
    math::Vec4 uvTransform{};
    float uvRotation = 0.f;
    float texCoordIndex = 0.f;
    float flipV = 0.f;
};

struct ForwardFsPush {
    int useIBL = 0;
    int useAlbedoTexture = 0;
};

struct ShadowVsPush {
    math::Mat4 model;
};

struct EntityIdVsPush {
    math::Mat4 model;
    math::Mat4 view;
    math::Mat4 projection;
};

struct EntityIdFragPush {
    uint32_t entityId = 0u;
};

static_assert(sizeof(ForwardVsPush) == 92u, "ForwardVsPush ABI mismatch.");
static_assert(sizeof(ForwardFsPush) == 8u, "ForwardFsPush ABI mismatch.");
static_assert(sizeof(ShadowVsPush) == sizeof(math::Mat4), "ShadowVsPush ABI mismatch.");
static_assert(sizeof(EntityIdVsPush) == sizeof(math::Mat4) * 3u, "EntityIdVsPush ABI mismatch.");
static_assert(sizeof(EntityIdFragPush) == sizeof(uint32_t), "EntityIdFragPush ABI mismatch.");

inline void WriteForwardPushConstants(rhi::ICommandList* cmd, const math::Mat4& model, int useIBL, int useAlbedoTexture,
                                      const math::Vec4& uvTransform = math::Vec4(0.f, 0.f, 1.f, 1.f),
                                      float uvRotation = 0.f, float texCoordIndex = 0.f, float flipV = 0.f) {
    if (!cmd) return;
    ForwardVsPush vsPush{};
    vsPush.model = model;
    vsPush.uvTransform = uvTransform;
    vsPush.uvRotation = uvRotation;
    vsPush.texCoordIndex = texCoordIndex;
    vsPush.flipV = flipV;
    const ForwardFsPush fsPush{ useIBL, useAlbedoTexture };
    cmd->SetPushConstants(kForwardVsOffset, static_cast<uint32_t>(sizeof(vsPush)), &vsPush);
    cmd->SetPushConstants(kForwardFsOffset, static_cast<uint32_t>(sizeof(fsPush)), &fsPush);
}

inline void WriteShadowPushConstants(rhi::ICommandList* cmd, const math::Mat4& model) {
    if (!cmd) return;
    const ShadowVsPush vsPush{ model };
    cmd->SetPushConstants(kShadowVsOffset, static_cast<uint32_t>(sizeof(vsPush)), &vsPush);
}

inline void WriteEntityIdViewProjectionPushConstants(rhi::ICommandList* cmd, const math::Mat4& view, const math::Mat4& projection) {
    if (!cmd) return;
    cmd->SetPushConstants(kEntityIdViewOffset, static_cast<uint32_t>(sizeof(math::Mat4)), &view);
    cmd->SetPushConstants(kEntityIdProjectionOffset, static_cast<uint32_t>(sizeof(math::Mat4)), &projection);
}

inline void WriteEntityIdModelPushConstants(rhi::ICommandList* cmd, const math::Mat4& model) {
    if (!cmd) return;
    cmd->SetPushConstants(kEntityIdModelOffset, static_cast<uint32_t>(sizeof(math::Mat4)), &model);
}

inline void WriteEntityIdFragPushConstants(rhi::ICommandList* cmd, uint32_t entityId) {
    if (!cmd) return;
    const EntityIdFragPush fsPush{ entityId };
    cmd->SetPushConstants(kEntityIdFragOffset, static_cast<uint32_t>(sizeof(fsPush)), &fsPush);
}

inline void WriteIblVertexPushConstants(rhi::ICommandList* cmd, const math::Mat4& projection, const math::Mat4& view) {
    if (!cmd) return;
    cmd->SetPushConstants(kIblProjectionOffset, static_cast<uint32_t>(sizeof(math::Mat4)), &projection);
    cmd->SetPushConstants(kIblViewOffset, static_cast<uint32_t>(sizeof(math::Mat4)), &view);
}

inline void WriteIblPrefilterRoughnessPushConstant(rhi::ICommandList* cmd, float roughness) {
    if (!cmd) return;
    cmd->SetPushConstants(kIblRoughnessOffset, sizeof(float), &roughness);
}

inline void WriteOutlinePostPushConstants(rhi::ICommandList* cmd, uint32_t selectedId, const math::Vec3& outlineColor,
                                          const math::Vec2& invSize) {
    if (!cmd) return;
    cmd->SetPushConstants(kOutlinePostSelectedIdOffset, sizeof(uint32_t), &selectedId);
    cmd->SetPushConstants(kOutlinePostColorOffset, sizeof(float) * 3u, &outlineColor);
    cmd->SetPushConstants(kOutlinePostInvSizeOffset, sizeof(float) * 2u, &invSize);
}

inline void WriteViewProjectionPairPushConstants(rhi::ICommandList* cmd, const math::Mat4& view, const math::Mat4& projection) {
    if (!cmd) return;
    cmd->SetPushConstants(kViewProjPairViewOffset, static_cast<uint32_t>(sizeof(math::Mat4)), &view);
    cmd->SetPushConstants(kViewProjPairProjectionOffset, static_cast<uint32_t>(sizeof(math::Mat4)), &projection);
}

inline void WriteSkyboxInversePushConstants(rhi::ICommandList* cmd, const math::Mat4& inverseView, const math::Mat4& inverseProjection,
                                            float skyboxBrightness) {
    if (!cmd) return;
    cmd->SetPushConstants(kSkyboxInverseViewOffset, static_cast<uint32_t>(sizeof(math::Mat4)), &inverseView);
    cmd->SetPushConstants(kSkyboxInverseProjectionOffset, static_cast<uint32_t>(sizeof(math::Mat4)), &inverseProjection);
    cmd->SetPushConstants(kSkyboxBrightnessOffset, static_cast<uint32_t>(sizeof(float)), &skyboxBrightness);
}

inline void WriteSkyboxInversePushConstants(rhi::ICommandList* cmd, const math::Mat4& inverseView, const math::Mat4& inverseProjection) {
    WriteSkyboxInversePushConstants(cmd, inverseView, inverseProjection, 1.0f);
}

/** GPU cull with draw list (`passes/gpu_cull/cull_indirect_with_drawlist_rs.comp`), std430. */
struct GpuCullPush {
    uint32_t numDraws = 0u;
    int32_t useOcclusionCull = 0;
    float depthSize[2] = { 0.f, 0.f };
};

static_assert(sizeof(GpuCullPush) == 16u, "GpuCullPush ABI mismatch.");

/** Streaming RGBA→NV12 (`passes/streaming/rgba_to_nv12.comp`), std430. */
struct RgbaToNv12Push {
    uint32_t width = 0u;
    uint32_t height = 0u;
    uint32_t pitch = 0u;
};

static_assert(sizeof(RgbaToNv12Push) == 12u, "RgbaToNv12Push ABI mismatch.");

inline void WriteGpuCullPushConstants(rhi::ICommandList* cmd, uint32_t numDraws, int useOcclusionCull, float depthSizeX,
                                      float depthSizeY) {
    if (!cmd) return;
    GpuCullPush p{};
    p.numDraws = numDraws;
    p.useOcclusionCull = useOcclusionCull;
    p.depthSize[0] = depthSizeX;
    p.depthSize[1] = depthSizeY;
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

inline void WriteRgbaToNv12PushConstants(rhi::ICommandList* cmd, uint32_t width, uint32_t height, uint32_t pitch) {
    if (!cmd) return;
    const RgbaToNv12Push p{ width, height, pitch };
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

/** Matches `GBufferPush` in `passes/deferred/gbuffer.vert` (std430). */
struct GBufferVertPushStd430 {
    math::Mat4 model{};
    math::Vec4 uvTransform{};
    float uvRotation = 0.f;
    float texCoordIndex = 0.f;
    float flipV = 0.f;
};

static_assert(sizeof(GBufferVertPushStd430) == 92u, "GBufferVertPushStd430 ABI mismatch.");

inline void WriteGBufferGeometryPushConstants(rhi::ICommandList* cmd, const math::Mat4& model, const math::Vec4& uvTransform,
                                              float uvRotation, float texCoordIndex, float flipV, float hasAlbedoTexture) {
    if (!cmd) return;
    GBufferVertPushStd430 vs{};
    vs.model = model;
    vs.uvTransform = uvTransform;
    vs.uvRotation = uvRotation;
    vs.texCoordIndex = texCoordIndex;
    vs.flipV = flipV;
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(vs)), &vs);
    cmd->SetPushConstants(kGBufferFragHasAlbedoOffset, sizeof(float), &hasAlbedoTexture);
}

/** Matches `GBufferInstancedPush` in `passes/deferred/gbuffer_instanced.vert` (std430). */
struct GBufferInstancedVertPushStd430 {
    math::Vec4 uvTransform{};
    float uvRotation = 0.f;
    float texCoordIndex = 0.f;
    float flipV = 0.f;
};

static_assert(sizeof(GBufferInstancedVertPushStd430) == 28u, "GBufferInstancedVertPushStd430 ABI mismatch.");

inline void WriteGBufferInstancedGeometryPushConstants(rhi::ICommandList* cmd, const math::Vec4& uvTransform, float uvRotation,
                                                       float texCoordIndex, float flipV, float hasAlbedoTexture) {
    if (!cmd) return;
    GBufferInstancedVertPushStd430 vs{};
    vs.uvTransform = uvTransform;
    vs.uvRotation = uvRotation;
    vs.texCoordIndex = texCoordIndex;
    vs.flipV = flipV;
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(vs)), &vs);
    cmd->SetPushConstants(kGBufferInstancedFragHasAlbedoOffset, sizeof(float), &hasAlbedoTexture);
}

/** `gbuffer_skinned.vert` bool-like int selector for BoneOrTransformBlock mode. */
inline void WriteGBufferSkinnedUsePerDrawTransformPushConstant(rhi::ICommandList* cmd, int32_t usePerDrawTransformBuffer) {
    if (!cmd) return;
    cmd->SetPushConstants(kGBufferSkinnedUsePerDrawTransformOffset, sizeof(int32_t), &usePerDrawTransformBuffer);
}

/** `passes/deferred/lighting.frag`: `layout(push_constant) uniform LightingPush { int u_UseIBL; }`. */
inline void WriteDeferredLightingUseIBL(rhi::ICommandList* cmd, int useIBL) {
    if (!cmd) return;
    cmd->SetPushConstants(0u, sizeof(int), &useIBL);
}

/** `passes/post/depth_pyramid.frag` push block (std430). */
struct DepthPyramidPushStd430 {
    int32_t u_Level = 0;
    int32_t _pad0 = 0;
    float u_SourceSize[2] = { 0.f, 0.f };
    float u_NearPlane = 0.f;
    float u_FarPlane = 0.f;
};

static_assert(sizeof(DepthPyramidPushStd430) == 24u, "DepthPyramidPushStd430 ABI mismatch.");

inline void WriteDepthPyramidPushConstants(rhi::ICommandList* cmd, int level, float sourceSizeX, float sourceSizeY, float nearPlane,
                                           float farPlane) {
    if (!cmd) return;
    DepthPyramidPushStd430 p{};
    p.u_Level = level;
    p.u_SourceSize[0] = sourceSizeX;
    p.u_SourceSize[1] = sourceSizeY;
    p.u_NearPlane = nearPlane;
    p.u_FarPlane = farPlane;
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

/** `passes/post/ssr.frag` SSRPush (std430); size rounded to 16-byte multiple. */
struct SSRPushStd430 {
    math::Mat4 u_ViewMatrix{};
    math::Mat4 u_ProjectionMatrix{};
    math::Mat4 u_InverseViewMatrix{};
    math::Vec4 u_ViewPos_NearFar{};
    float u_Resolution[2] = { 0.f, 0.f };
    float u_StepSize = 0.f;
    float u_Thickness = 0.f;
    float u_FarPlane = 0.f;
    int32_t u_MaxSteps = 0;
    uint8_t _pad[8]{};
};

static_assert(sizeof(SSRPushStd430) == 240u, "SSRPushStd430 ABI mismatch.");

inline void WriteSSRPushConstants(rhi::ICommandList* cmd, const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                                  const math::Mat4& inverseViewMatrix, const math::Vec3& viewPos, float nearPlane, float farPlane,
                                  float resolutionX, float resolutionY, float stepSize, float thickness, int maxSteps) {
    if (!cmd) return;
    SSRPushStd430 p{};
    p.u_ViewMatrix = viewMatrix;
    p.u_ProjectionMatrix = projectionMatrix;
    p.u_InverseViewMatrix = inverseViewMatrix;
    p.u_ViewPos_NearFar = math::Vec4(viewPos.x, viewPos.y, viewPos.z, nearPlane);
    p.u_Resolution[0] = resolutionX;
    p.u_Resolution[1] = resolutionY;
    p.u_StepSize = stepSize;
    p.u_Thickness = thickness;
    p.u_FarPlane = farPlane;
    p.u_MaxSteps = maxSteps;
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

/** Post fullscreen passes: resource-space PostPass set + push at offset 0 (`passes/post/*`). */

struct BloomCompositePush {
    float bloomIntensity = 0.f;
};

static_assert(sizeof(BloomCompositePush) == 4u, "BloomCompositePush ABI mismatch.");

inline void WriteBloomCompositePushConstant(rhi::ICommandList* cmd, float bloomIntensity) {
    if (!cmd) return;
    const BloomCompositePush p{ bloomIntensity };
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

struct TonemapPushStd430 {
    float exposure = 0.f;
    int32_t useACES = 0;
};

static_assert(sizeof(TonemapPushStd430) == 8u, "TonemapPushStd430 ABI mismatch.");

inline void WriteTonemapPushConstants(rhi::ICommandList* cmd, float exposure, int useACES) {
    if (!cmd) return;
    TonemapPushStd430 p{};
    p.exposure = exposure;
    p.useACES = useACES ? 1 : 0;
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

struct BloomExtractPush {
    float threshold = 0.f;
};

static_assert(sizeof(BloomExtractPush) == 4u, "BloomExtractPush ABI mismatch.");

inline void WriteBloomExtractPushConstant(rhi::ICommandList* cmd, float threshold) {
    if (!cmd) return;
    const BloomExtractPush p{ threshold };
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

struct BloomBlurPushStd430 {
    float resolution[2] = { 0.f, 0.f };
    int32_t horiz = 0;
    float intensity = 0.f;
};

static_assert(sizeof(BloomBlurPushStd430) == 16u, "BloomBlurPushStd430 ABI mismatch.");

inline void WriteBloomBlurPushConstants(rhi::ICommandList* cmd, float resolutionX, float resolutionY, int horiz01, float intensity) {
    if (!cmd) return;
    BloomBlurPushStd430 p{};
    p.resolution[0] = resolutionX;
    p.resolution[1] = resolutionY;
    p.horiz = horiz01 ? 1 : 0;
    p.intensity = intensity;
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

struct ChromaticAberrationPushStd430 {
    float resolution[2] = { 0.f, 0.f };
    float intensity = 0.f;
};

static_assert(sizeof(ChromaticAberrationPushStd430) == 12u, "ChromaticAberrationPushStd430 ABI mismatch.");

inline void WriteChromaticAberrationPushConstants(rhi::ICommandList* cmd, float resolutionX, float resolutionY, float intensity) {
    if (!cmd) return;
    ChromaticAberrationPushStd430 p{};
    p.resolution[0] = resolutionX;
    p.resolution[1] = resolutionY;
    p.intensity = intensity;
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

struct FxaaPushStd430 {
    float resolution[2] = { 0.f, 0.f };
};

static_assert(sizeof(FxaaPushStd430) == 8u, "FxaaPushStd430 ABI mismatch.");

inline void WriteFxaaPushConstants(rhi::ICommandList* cmd, float resolutionX, float resolutionY) {
    if (!cmd) return;
    FxaaPushStd430 p{};
    p.resolution[0] = resolutionX;
    p.resolution[1] = resolutionY;
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

struct SsaoPushStd430 {
    math::Mat4 view{};
    math::Mat4 projection{};
    float resolution[2] = { 0.f, 0.f };
    float radius = 0.f;
    float bias = 0.f;
    int32_t sampleCount = 0;
};

static_assert(sizeof(SsaoPushStd430) == 148u, "SsaoPushStd430 ABI mismatch.");

inline void WriteSsaoPushConstants(rhi::ICommandList* cmd, const math::Mat4& viewMatrix, const math::Mat4& projectionMatrix,
                                   float resolutionX, float resolutionY, float radius, float bias, int sampleCount) {
    if (!cmd) return;
    SsaoPushStd430 p{};
    p.view = viewMatrix;
    p.projection = projectionMatrix;
    p.resolution[0] = resolutionX;
    p.resolution[1] = resolutionY;
    p.radius = radius;
    p.bias = bias;
    p.sampleCount = sampleCount;
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

constexpr uint32_t kSsaoBlurKernelFloats = 16u;

struct SsaoBlurPushStd430 {
    float axis[2] = { 0.f, 0.f };
    float resolution[2] = { 0.f, 0.f };
    float nearPlane = 0.f;
    float farPlane = 0.f;
    float farOverEdgeDistance = 0.f;
    int32_t sampleCount = 0;
    float kernel[16] = {};
};

static_assert(sizeof(SsaoBlurPushStd430) == 96u, "SsaoBlurPushStd430 ABI mismatch.");

inline void WriteSsaoBlurPushConstants(rhi::ICommandList* cmd, float axisX, float axisY, float resolutionX, float resolutionY,
                                       float nearPlane, float farPlane, float farOverEdgeDistance, int32_t sampleCount,
                                       const float* kernel, int kernelLength) {
    if (!cmd) return;
    SsaoBlurPushStd430 p{};
    p.axis[0] = axisX;
    p.axis[1] = axisY;
    p.resolution[0] = resolutionX;
    p.resolution[1] = resolutionY;
    p.nearPlane = nearPlane;
    p.farPlane = farPlane;
    p.farOverEdgeDistance = farOverEdgeDistance;
    p.sampleCount = sampleCount;
    const int maxK = static_cast<int>(kSsaoBlurKernelFloats);
    for (int i = 0; i < maxK && i < kernelLength; ++i)
        p.kernel[i] = kernel[i];
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

struct MotionBlurPushStd430 {
    float resolution[2] = { 0.f, 0.f };
    float intensity = 0.f;
    int32_t samples = 0;
};

static_assert(sizeof(MotionBlurPushStd430) == 16u, "MotionBlurPushStd430 ABI mismatch.");

inline void WriteMotionBlurPushConstants(rhi::ICommandList* cmd, float resolutionX, float resolutionY, float intensity, int samples) {
    if (!cmd) return;
    MotionBlurPushStd430 p{};
    p.resolution[0] = resolutionX;
    p.resolution[1] = resolutionY;
    p.intensity = intensity;
    p.samples = samples;
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

struct VignettePushStd430 {
    float intensity = 0.f;
    float radius = 0.f;
    float softness = 0.f;
};

static_assert(sizeof(VignettePushStd430) == 12u, "VignettePushStd430 ABI mismatch.");

inline void WriteVignettePushConstants(rhi::ICommandList* cmd, float intensity, float radius, float softness) {
    if (!cmd) return;
    const VignettePushStd430 p{ intensity, radius, softness };
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

struct FilmGrainPushStd430 {
    float intensity = 0.f;
    float time = 0.f;
};

static_assert(sizeof(FilmGrainPushStd430) == 8u, "FilmGrainPushStd430 ABI mismatch.");

inline void WriteFilmGrainPushConstants(rhi::ICommandList* cmd, float intensity, float time) {
    if (!cmd) return;
    const FilmGrainPushStd430 p{ intensity, time };
    cmd->SetPushConstants(0u, static_cast<uint32_t>(sizeof(p)), &p);
}

} // namespace abi
} // namespace rendering
} // namespace eyelib
