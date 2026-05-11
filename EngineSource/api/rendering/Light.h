#pragma once

#include "eyelib/math/Types.h"

#include <cstdint>

// =====================================================================================
// api/rendering/Light.h — public POD descriptors for scene lights (M4.0a).
//
// Mirrors `eyelib::rendering::SceneLighting::*State` 1:1 in field shape so that editor /
// player code populating `RenderScene` does a structural copy at the boundary. The
// `RenderPipeline::Render` adapter pushes these into the new module's `SceneLighting`
// each frame (clear + re-add point + spot lights, set directional).
//
// Why three separate types instead of one tagged-union `Light`:
//  - The new module's `SceneLighting` already separates the three (one directional slot,
//    a vector of point lights, a vector of spot lights). Mirroring the same shape on the
//    public side keeps the adapter trivial and the cap behaviour visible to consumers
//    (kMaxScenePointLights = 8 on the new module — extras are silently dropped).
//  - Directional has a dedicated single-slot scene state (no array semantics).
//
// Numeric units / conventions:
//  - `direction` is a unit vector (the adapter normalises before forwarding); the legacy
//    player BuildScene already normalises so this is redundant but kept defensive.
//  - `color` is linear RGB (0..1 typical); intensity is a multiplier (legacy player uses
//    1..10 range typically; same convention).
//  - `range` (point / spot) is in world units; the new module's lighting bus consumes the
//    same unit.
//  - `innerConeCos` / `outerConeCos` (spot) are pre-computed cosines (legacy player calls
//    `cos(degToRad(angle))` before filling these); the new module also expects cosines.
//  - `castShadows` toggles per-light shadow rendering. The new module's `ShadowFeature`
//    consults the same flag.
//
// Lights and skybox are intentionally NOT tied into the public `Material` PIMPL; they
// flow through `RenderScene` once per frame.
// =====================================================================================

namespace eyelib {
namespace api {
namespace rendering {

/** Single directional light in the scene (sun / key light analogue). */
struct DirectionalLight {
    /** Unit-vector direction the light travels (e.g. {0, -1, 0} = top-down). */
    math::Vec3 direction = {-0.35f, -0.85f, -0.25f};
    /** Linear RGB color (0..1 typical). */
    math::Vec3 color     = {1.0f, 0.95f, 0.85f};
    /** Multiplier on `color`; 0 = effectively off. */
    float      intensity = 5.0f;
    /** Cast directional CSM shadows. */
    bool       castShadows = true;
};

/** Point light (omnidirectional, falloff at `range`). */
struct PointLight {
    math::Vec3 position    = {0.0f, 3.0f, 0.0f};
    math::Vec3 color       = {1.0f, 1.0f, 1.0f};
    float      intensity   = 10.0f;
    float      range       = 15.0f;
    bool       castShadows = true;
};

/** Spot light (cone, falloff between inner and outer cone cosines). */
struct SpotLight {
    math::Vec3 position     = {0.0f, 5.0f, 0.0f};
    math::Vec3 direction    = {0.0f, -1.0f, 0.0f};
    math::Vec3 color        = {1.0f, 1.0f, 1.0f};
    float      intensity    = 10.0f;
    float      range        = 20.0f;
    /** cos(innerConeAngle) — full intensity inside this cone. */
    float      innerConeCos = 0.9f;
    /** cos(outerConeAngle) — falloff to zero at this cone edge. innerConeCos > outerConeCos. */
    float      outerConeCos = 0.8f;
    bool       castShadows  = true;
};

}  // namespace rendering
}  // namespace api
}  // namespace eyelib
