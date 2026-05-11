#include "EditorSceneBuilderNew.h"

#include "api/asset/AssetRegistry.h"
#include "api/core/Log.h"
#include "api/rendering/Material.h"
#include "api/rendering/Mesh.h"
#include "api/rendering/ResourceCache.h"

#include "eyelib/math/AABB.h"
#include "eyelib/math/Types.h"
#include "eyelib/math/TransformUtils.h"

#include <atomic>
#include <cmath>

namespace openeye::editor {

namespace {

constexpr float kPi = 3.14159265358979323846f;

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

/**
 * Emit a one-shot warning the first time the new editor scene builder runs without a real
 * GPU resource cache backing the new façade. Lets devs see in logs when this code path is
 * exercised before M3.7 lands; never fires more than once per process.
 */
void LogResourceResolutionGapOnce() {
    static std::atomic<bool> warned{false};
    bool expected = false;
    if (warned.compare_exchange_strong(expected, true)) {
        eyelib::api::Log::Warn(
            "EditorSceneBuilderNew: scene built with placeholder mesh/material handles; "
            "real handles wire in when EditorGPUResourceCache moves to api/rendering.");
    }
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
                                  eyelib::api::rendering::ResourceCache* resourceCacheNew) {
    (void)assetRoots; // Reserved: M3.9 may need asset-root-relative path resolution.

    scene.opaque.clear();
    scene.transparent.clear();
    if (outDrawableToEntity)
        outDrawableToEntity->clear();
    if (outTransparentDrawableToEntity)
        outTransparentDrawableToEntity->clear();

    BuildEditorCameraView(editorCamera, viewportWidth, viewportHeight, scene.camera);

    const auto getTransform = [&](::eyelib::world::EntityId id) {
        return snapshot.GetTransform(id);
    };

    auto emitDrawable = [&](::eyelib::world::EntityId entity,
                            const std::string& meshAssetId,
                            const std::string& materialAssetId,
                            int32_t /*submeshIndex*/) {
        const auto transform = getTransform(entity);
        if (!transform) return;

        eyelib::api::rendering::DrawableInstance instance{};

        // M3.7 — when the new-API resource cache is wired (M3.9 closeout flip),
        // resolve asset ids → opaque GPU tokens. Until then, the cache returns
        // invalid handles (one-shot warning) and the placeholder zero tokens are
        // preserved, matching the M2 / M3 cold-path intent.
        if (resourceCacheNew && assetRegistry) {
            if (!meshAssetId.empty()) {
                eyelib::api::AssetId mid = assetRegistry->GetId(meshAssetId);
                if (mid.IsValid()) {
                    eyelib::api::rendering::Mesh mesh = resourceCacheNew->GetMesh(mid);
                    instance.meshHandle = mesh.GetMeshHandle();
                }
            }
            if (!materialAssetId.empty()) {
                eyelib::api::AssetId matId = assetRegistry->GetId(materialAssetId);
                if (matId.IsValid()) {
                    eyelib::api::rendering::Material mat = resourceCacheNew->GetMaterial(matId);
                    instance.materialId = mat.GetMaterialId();
                }
            }
        }

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

    if (!scene.opaque.empty() || !scene.transparent.empty())
        LogResourceResolutionGapOnce();
}

} // namespace openeye::editor
