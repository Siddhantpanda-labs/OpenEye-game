#include "EditorSceneBuilderNew.h"

#include "BuiltinResourceManagerNew.h"
#include "RendererGPUResourceCacheNew.h"

#include "api/asset/AssetRegistry.h"
#include "api/geometry/BuiltinMeshIds.h"
#include "api/rendering/Material.h"
#include "api/rendering/Mesh.h"

#include "eyelib/math/AABB.h"
#include "eyelib/math/Types.h"
#include "eyelib/math/TransformUtils.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>

#include <cmath>

namespace openeye::editor {

namespace {

constexpr float kPi = 3.14159265358979323846f;

inline float DegreesToRadians(float degrees) { return degrees * (kPi / 180.0f); }

/** Rotate the canonical forward (0,0,-1) by a quaternion. Matches the player's
 *  `WorldRenderBridge::ForwardFromRotation` so light direction conventions match across
 *  editor + player. */
inline eyelib::math::Vec3 ForwardFromRotation(const eyelib::math::Quat& rotation) {
    const eyelib::math::Vec3 forward(0.0f, 0.0f, -1.0f);
    return rotation * forward;
}

/**
 * Self-contained free-fly camera math. Inlined here (rather than reusing the legacy
 * `eyelib::api::camera::BuildFreeFlyCameraView` helper from `api/render/CameraConventions.h`)
 * because this translation unit must stay clean of legacy-API includes — see class docstring.
 *
 * Vulkan conventions baked in (the new module is Vulkan-only): clip-space Y is flipped
 * relative to GL by negating projection[1][1]; the depth range is GL-style [-1, 1] returned
 * by `eyelib::math::Perspective`, with the new module performing the depth remap inside the
 * shader pipeline (matches the legacy `CameraPolicy::ZeroToOne` post-bake on Vulkan).
 */
void BuildEditorCameraView(const EditorCamera& cam,
                           uint32_t viewportWidth,
                           uint32_t viewportHeight,
                           eyelib::api::rendering::CameraView& out) {
    const float w = static_cast<float>(viewportWidth);
    const float h = static_cast<float>(viewportHeight);
    const float aspect = (h > 0.0f) ? (w / h) : 1.0f;
    const float fovRadians = cam.fovDegrees * kPi / 180.0f;

    const eyelib::math::Vec3 position(cam.position[0], cam.position[1], cam.position[2]);

    // Forward vector from yaw (around world Y) + pitch (around camera X).
    const float cy = std::cos(cam.pitch);
    const float sy = std::sin(cam.pitch);
    const float ct = std::cos(cam.yaw);
    const float st = std::sin(cam.yaw);
    const eyelib::math::Vec3 forward(cy * st, sy, -cy * ct);
    const eyelib::math::Vec3 target = position + forward;

    out.nearPlane = 0.1f;
    out.farPlane = 1000.0f;
    out.viewPos = position;
    out.view = eyelib::math::LookAt(position, target, eyelib::math::Vec3(0.0f, 1.0f, 0.0f));
    out.projection = eyelib::math::Perspective(fovRadians, aspect, out.nearPlane, out.farPlane);

    // Vulkan clip-space Y flip (matches legacy AdjustCameraForBackend(Vulkan)).
    out.projection[1][1] = -out.projection[1][1];
}

eyelib::math::Mat4 TransformToMatrix(const ::eyelib::world::components::Transform& t) {
    return eyelib::math::Compose(t.position, t.rotation, t.scale);
}

} // namespace

void EditorSceneBuilderNew::Build(eyelib::api::rendering::RenderScene& scene,
                                  const eyelib::api::world::World::RenderSnapshot& snapshot,
                                  const EditorCamera& editorCamera,
                                  uint32_t viewportWidth,
                                  uint32_t viewportHeight,
                                  const std::vector<std::string>& assetRoots,
                                  eyelib::api::AssetRegistry* assetRegistry,
                                  std::vector<::eyelib::world::EntityId>* outDrawableToEntity,
                                  std::vector<::eyelib::world::EntityId>* outTransparentDrawableToEntity,
                                  openeye::player::RendererGPUResourceCacheNew* gpuResolverNew,
                                  openeye::player::BuiltinResourceManagerNew*   builtinManagerNew) {
    (void)assetRoots; // Reserved: M3.9 may need asset-root-relative path resolution.

    scene.opaque.clear();
    scene.transparent.clear();
    scene.pointLights.clear();
    scene.spotLights.clear();
    scene.directionalLightActive = false;
    if (outDrawableToEntity)
        outDrawableToEntity->clear();
    if (outTransparentDrawableToEntity)
        outTransparentDrawableToEntity->clear();

    BuildEditorCameraView(editorCamera, viewportWidth, viewportHeight, scene.camera);

    const auto getTransform = [&](::eyelib::world::EntityId id) {
        return snapshot.GetTransform(id);
    };

    // Mesh/material resolution mirrors `WorldRenderBridge` exactly so editor + player share
    // one path (single source of truth). Built-in primitives via the builtin manager;
    // file assets via the resolver (uploads on miss); default material fallback when the
    // entity has no material set or it failed to resolve.
    auto resolveHandles = [&](const std::string& meshKey, const std::string& materialAssetId,
                              uint32_t& meshHandleOut, uint32_t& materialIdOut) {
        meshHandleOut = 0;
        materialIdOut = 0;
        if (eyelib::api::geometry::IsBuiltinMeshId(meshKey) && builtinManagerNew) {
            auto entry = builtinManagerNew->GetPrimitiveEntry(meshKey);
            meshHandleOut = entry.mesh.GetMeshHandle();
            materialIdOut = entry.material.GetMaterialId();
        } else if (gpuResolverNew && assetRegistry && !meshKey.empty()) {
            const eyelib::api::AssetId mid = assetRegistry->GetId(meshKey);
            if (mid.IsValid())
                meshHandleOut = gpuResolverNew->GetMesh(mid).GetMeshHandle();
        }
        if (!materialAssetId.empty() && gpuResolverNew && assetRegistry) {
            const eyelib::api::AssetId matId = assetRegistry->GetId(materialAssetId);
            if (matId.IsValid())
                materialIdOut = gpuResolverNew->GetMaterial(matId).GetMaterialId();
        }
        if (materialIdOut == 0 && builtinManagerNew) {
            materialIdOut = builtinManagerNew->GetDefaultMaterial().GetMaterialId();
        }
    };

    auto emitDrawable = [&](::eyelib::world::EntityId entity,
                            const std::string& meshAssetId,
                            const std::string& materialAssetId,
                            int32_t submeshIndex) {
        const auto transform = getTransform(entity);
        if (!transform) return;

        eyelib::api::rendering::DrawableInstance instance{};
        resolveHandles(meshAssetId, materialAssetId, instance.meshHandle, instance.materialId);
        instance.submeshIndex = submeshIndex;

        instance.worldTransform = TransformToMatrix(*transform);
        instance.localBounds = eyelib::math::AABB{};
        instance.layer = eyelib::api::rendering::DrawLayer::Opaque;
        instance.flags = eyelib::api::rendering::draw_instance_flags::kCastShadow;

        // Routing: until material introspection is wired through the new façade
        // (transparent shading model lookup needs M3.2's MaterialDescriptor migration),
        // every visible drawable goes to the opaque bucket. Transparent bucket stays
        // empty for v1 — matches the M2 "scaffold-complete cold path" intent.
        scene.opaque.push_back(instance);
        if (outDrawableToEntity)
            outDrawableToEntity->push_back(entity);
    };

    for (const auto& entry : snapshot.renderables) {
        const auto& renderable = entry.component;
        if (!renderable.visible) continue;
        std::string meshId = renderable.meshAssetId;
        if (!renderable.lodMeshAssetIds.empty() &&
            renderable.activeLOD < renderable.lodMeshAssetIds.size())
            meshId = renderable.lodMeshAssetIds[renderable.activeLOD];
        emitDrawable(entry.entity, meshId, renderable.materialAssetId, renderable.submeshIndex);
    }

    for (const auto& entry : snapshot.meshRenderers) {
        const auto& renderer = entry.component;
        if (!renderer.visible || renderer.meshAssetId.empty()) continue;
        emitDrawable(entry.entity, renderer.meshAssetId, renderer.materialAssetId, renderer.submeshIndex);
    }

    // Lights — mirror the player's `WorldRenderBridge` directional/point/spot conversion so
    // scene-view + player share one convention. First directional wins (matches legacy
    // single-slot semantics); point + spot accumulate (capped downstream by SceneLighting).
    bool directionalSet = false;
    for (const auto& entry : snapshot.lights) {
        const auto& light = entry.component;
        const auto transform = getTransform(entry.entity);

        if (light.type == ::eyelib::world::components::Light::Type::Directional && !directionalSet) {
            eyelib::math::Vec3 direction(0.5f, -1.0f, 0.2f);
            if (transform)
                direction = ForwardFromRotation(transform->rotation);
            const auto nd = glm::normalize(glm::vec3(direction.x, direction.y, direction.z));
            scene.directionalLight.direction = eyelib::math::Vec3(nd.x, nd.y, nd.z);
            scene.directionalLight.color = light.color;
            scene.directionalLight.intensity = light.intensity;
            scene.directionalLight.castShadows = light.castShadows;
            scene.directionalLightActive = true;
            directionalSet = true;
        } else if (light.type == ::eyelib::world::components::Light::Type::Point) {
            eyelib::api::rendering::PointLight point{};
            if (transform)
                point.position = transform->position;
            point.color = light.color;
            point.intensity = light.intensity;
            point.range = light.range;
            point.castShadows = light.castShadows;
            scene.pointLights.push_back(point);
        } else if (light.type == ::eyelib::world::components::Light::Type::Spot) {
            eyelib::api::rendering::SpotLight spot{};
            if (transform) {
                spot.position = transform->position;
                const auto gd = glm::normalize(ForwardFromRotation(transform->rotation));
                spot.direction = eyelib::math::Vec3(gd.x, gd.y, gd.z);
            }
            spot.color = light.color;
            spot.intensity = light.intensity;
            spot.range = light.range;
            spot.innerConeCos = std::cos(DegreesToRadians(light.innerConeAngle));
            spot.outerConeCos = std::cos(DegreesToRadians(light.outerConeAngle));
            spot.castShadows = light.castShadows;
            scene.spotLights.push_back(spot);
        }
    }
}

} // namespace openeye::editor
