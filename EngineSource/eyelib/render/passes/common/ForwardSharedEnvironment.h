#pragma once

#include "eyelib/render/passes/common/ForwardSharedStateWriter.h"

namespace eyelib {
namespace render {
namespace passes {
namespace common {

/** Assembles resolve params from frame/camera and delegates to ResolveForwardSharedEnvironment. */
inline ForwardSharedEnvironmentState ResolveForwardSharedEnvironmentForPipeline(
    const RenderFrameData& frameData,
    const CameraView& camera,
    CascadeShadowMap* cascadeShadowMap,
    ReflectionProbeManager* reflectionProbeManager,
    IBL* ibl) {
    ForwardSharedEnvironmentResolveParams params{};
    params.light = &frameData.light;
    params.camera = &camera;
    params.cascadeShadowMap = cascadeShadowMap;
    params.reflectionProbeManager = reflectionProbeManager;
    params.ibl = ibl;
    return ResolveForwardSharedEnvironment(params);
}

}  // namespace common
}  // namespace passes
}  // namespace render
}  // namespace eyelib
