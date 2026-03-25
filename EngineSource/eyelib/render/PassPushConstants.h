#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/render/rhi/RHI.h"

namespace eyelib {
namespace render {
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

/** G-buffer geometry (`passes/deferred/gbuffer.vert` + `gbuffer.frag`), std430: VS block then FS float. */
constexpr uint32_t kGBufferFragHasAlbedoOffset = 92u;

struct ForwardVsPush {
    math::Mat4 model;
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

static_assert(sizeof(ForwardVsPush) == sizeof(math::Mat4), "ForwardVsPush ABI mismatch.");
static_assert(sizeof(ForwardFsPush) == 8u, "ForwardFsPush ABI mismatch.");
static_assert(sizeof(ShadowVsPush) == sizeof(math::Mat4), "ShadowVsPush ABI mismatch.");
static_assert(sizeof(EntityIdVsPush) == sizeof(math::Mat4) * 3u, "EntityIdVsPush ABI mismatch.");
static_assert(sizeof(EntityIdFragPush) == sizeof(uint32_t), "EntityIdFragPush ABI mismatch.");

inline void WriteForwardPushConstants(rhi::ICommandList* cmd, const math::Mat4& model, int useIBL, int useAlbedoTexture) {
    if (!cmd) return;
    const ForwardVsPush vsPush{ model };
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

inline void WriteSkyboxInversePushConstants(rhi::ICommandList* cmd, const math::Mat4& inverseView, const math::Mat4& inverseProjection) {
    if (!cmd) return;
    cmd->SetPushConstants(kSkyboxInverseViewOffset, static_cast<uint32_t>(sizeof(math::Mat4)), &inverseView);
    cmd->SetPushConstants(kSkyboxInverseProjectionOffset, static_cast<uint32_t>(sizeof(math::Mat4)), &inverseProjection);
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

} // namespace abi
} // namespace render
} // namespace eyelib
