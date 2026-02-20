#include "EditorSceneBuilder.h"

#include "api/assets/AssetRegistry.h"
#include "api/assets/loaders/ModelLoader.h"
#include "api/core/All.h"
#include "api/math/TransformUtils.h"
#include "api/platform/FileSystem.h"
#include "api/render/Material.h"
#include "api/render/RenderScene.h"
#include "api/render/Shader.h"

#include <cmath>
#include <exception>

namespace openeye::editor {

namespace {

static std::string JoinPath(const std::string& base, const std::string& leaf) {
    if (base.empty()) return leaf;
    const char last = base.back();
    if (last == '/' || last == '\\') return base + leaf;
    return base + "/" + leaf;
}

eyelib::math::Mat4 TransformToMatrix(const ::eyelib::world::components::Transform& t) {
    return eyelib::math::Compose(t.position, t.rotation, t.scale);
}

/** Editor free-fly camera: position + yaw/pitch -> view matrix. */
void EditorCameraToView(const EditorCamera& cam, uint32_t viewportWidth, uint32_t viewportHeight,
                       eyelib::api::CameraView& out) {
    const float aspect = (viewportHeight > 0) ? static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight) : 1.0f;
    out.nearPlane = 0.1f;
    out.farPlane = 1000.0f;
    out.projection = eyelib::math::Perspective(
        static_cast<float>(cam.fovDegrees * 3.14159265 / 180.0), aspect, out.nearPlane, out.farPlane);

    eyelib::math::Vec3 position(cam.position[0], cam.position[1], cam.position[2]);
    float cy = std::cos(cam.pitch);
    float sy = std::sin(cam.pitch);
    float ct = std::cos(cam.yaw);
    float st = std::sin(cam.yaw);
    // Forward direction: -Z when yaw=0 pitch=0 (match player/render convention so scene at origin is visible)
    eyelib::math::Vec3 forward(cy * st, sy, -cy * ct);
    eyelib::math::Vec3 target = position + forward;
    out.viewPos = position;
    out.view = eyelib::math::LookAt(position, target, eyelib::math::Vec3(0.0f, 1.0f, 0.0f));
}

} // namespace

std::string EditorSceneBuilder::ResolveAssetPath(const std::string& id, const std::vector<std::string>& roots) {
    if (id.empty()) return {};
    if (api::platform::filesystem::FileExists(id)) return id;
    for (const auto& root : roots) {
        const std::string candidate = JoinPath(root, id);
        if (api::platform::filesystem::FileExists(candidate)) return candidate;
    }
    return {};
}

void EditorSceneBuilder::EnsureDefaultMaterial() {
    if (m_defaultMaterial) return;
    auto shader = std::make_shared<eyelib::api::Shader>();
    if (shader->LoadFromFile("passes/forward/basic", "passes/forward/basic")) {
        auto mat = std::make_shared<eyelib::api::Material>();
        mat->SetShader(shader);
        m_defaultMaterial = mat;
        return;
    }
    eyelib::api::core::Log::Warn("EditorSceneBuilder: default shader missing; using empty material.");
    m_defaultMaterial = std::make_shared<eyelib::api::Material>();
}

EditorSceneBuilder::ModelEntry EditorSceneBuilder::LoadModelEntry(const std::string& path) {
    auto it = m_modelCache.find(path);
    if (it != m_modelCache.end()) return it->second;

    ModelEntry entry{};
    try {
        std::shared_ptr<eyelib::api::ModelAsset> model = eyelib::api::ModelLoader::LoadModel(path);
        if (!model) {
            m_modelCache.emplace(path, entry);
            return entry;
        }
        const auto& meshes = model->GetMeshes();
        const auto& materials = model->GetMaterials();
        if (!meshes.empty()) entry.mesh = meshes.front();
        if (!materials.empty()) entry.material = materials.front();
        if (!entry.material) {
            EnsureDefaultMaterial();
            entry.material = m_defaultMaterial;
        }
    } catch (const std::exception& e) {
        eyelib::api::core::Log::ErrorF("EditorSceneBuilder: failed to load model %s - %s", path.c_str(), e.what());
    } catch (...) {
        eyelib::api::core::Log::ErrorF("EditorSceneBuilder: failed to load model %s", path.c_str());
    }
    m_modelCache.emplace(path, entry);
    return entry;
}

void EditorSceneBuilder::Build(eyelib::api::RenderScene& scene,
                              const eyelib::api::world::World::RenderSnapshot& snapshot,
                              const EditorCamera& editorCamera,
                              uint32_t viewportWidth,
                              uint32_t viewportHeight,
                              const std::vector<std::string>& assetRoots,
                              eyelib::api::AssetRegistry* assetRegistry) {
    scene.opaqueStorage.clear();
    scene.transparentStorage.clear();
    scene.pointLightsStorage.clear();
    scene.spotLightsStorage.clear();

    EditorCameraToView(editorCamera, viewportWidth, viewportHeight, scene.camera);

    scene.light.direction = eyelib::math::Vec3(0.5f, -1.0f, 0.2f);
    scene.light.color = eyelib::math::Vec3(1.0f, 0.95f, 0.9f);
    scene.light.intensity = 3.0f;
    scene.light.castShadows = false;
    scene.skybox = nullptr;

    auto getTransform = [&](::eyelib::world::EntityId id) { return snapshot.GetTransform(id); };

    for (const auto& entry : snapshot.renderables) {
        const auto& renderable = entry.component;
        if (!renderable.visible) continue;
        std::string meshId = renderable.meshAssetId;
        if (!renderable.lodMeshAssetIds.empty() && renderable.activeLOD < renderable.lodMeshAssetIds.size())
            meshId = renderable.lodMeshAssetIds[renderable.activeLOD];
        const std::string meshResolved = ResolveAssetPath(meshId, assetRoots);
        if (meshResolved.empty()) continue;
        ModelEntry modelEntry = LoadModelEntry(meshResolved);
        if (!modelEntry.mesh) continue;

        eyelib::api::Drawable drawable{};
        drawable.mesh = modelEntry.mesh;
        drawable.material = modelEntry.material;
        if (!renderable.materialAssetId.empty() && assetRegistry) {
            const std::string matResolved = ResolveAssetPath(renderable.materialAssetId, assetRoots);
            if (!matResolved.empty()) {
                if (auto mat = assetRegistry->GetMaterial(matResolved))
                    drawable.material = mat;
            }
        }
        if (const auto transform = getTransform(entry.entity))
            drawable.transform = TransformToMatrix(*transform);
        if (drawable.material)
            eyelib::api::FillDrawableRenderParams(drawable.params, drawable.material, nullptr);
        scene.opaqueStorage.push_back(drawable);
    }

    for (const auto& entry : snapshot.meshRenderers) {
        const auto& renderer = entry.component;
        if (!renderer.visible || renderer.meshAssetId.empty()) continue;
        const std::string meshResolved = ResolveAssetPath(renderer.meshAssetId, assetRoots);
        if (meshResolved.empty()) continue;
        ModelEntry modelEntry = LoadModelEntry(meshResolved);
        if (!modelEntry.mesh) continue;

        eyelib::api::Drawable drawable{};
        drawable.mesh = modelEntry.mesh;
        drawable.material = modelEntry.material;
        if (!renderer.materialAssetId.empty() && assetRegistry) {
            const std::string matResolved = ResolveAssetPath(renderer.materialAssetId, assetRoots);
            if (!matResolved.empty()) {
                if (auto mat = assetRegistry->GetMaterial(matResolved))
                    drawable.material = mat;
            }
        }
        if (const auto transform = getTransform(entry.entity))
            drawable.transform = TransformToMatrix(*transform);
        if (drawable.material)
            eyelib::api::FillDrawableRenderParams(drawable.params, drawable.material, nullptr);
        scene.opaqueStorage.push_back(drawable);
    }

    scene.opaque = scene.opaqueStorage.empty() ? nullptr : scene.opaqueStorage.data();
    scene.opaqueCount = scene.opaqueStorage.size();
    scene.transparent = nullptr;
    scene.transparentCount = 0;
    scene.pointLights = nullptr;
    scene.pointLightCount = 0;
    scene.spotLights = nullptr;
    scene.spotLightCount = 0;
}

} // namespace openeye::editor
