#include "WorldRender.h"

#include "api/assets/loaders/ModelLoader.h"
#include "api/animation/Skeleton.h"
#include "api/core/All.h"
#include "api/math/TransformUtils.h"
#include "api/platform/All.h"
#include "api/render/RenderScene.h"
#include "WorldAnimation.h"

#include <cmath>
#include <exception>
#include <glm/glm.hpp>

namespace openeye::player {

namespace {

static std::string JoinPath(const std::string& base, const std::string& leaf) {
    if (base.empty()) return leaf;
    const char last = base.back();
    if (last == '/' || last == '\\') return base + leaf;
    return base + "/" + leaf;
}

static std::string ResolveAssetPath(const std::string& id, const std::vector<std::string>& roots) {
    using eyelib::api::platform::filesystem::FileExists;
    if (id.empty()) return {};
    if (FileExists(id)) return id;
    for (const auto& root : roots) {
        const std::string candidate = JoinPath(root, id);
        if (FileExists(candidate)) return candidate;
    }
    return {};
}

float DegreesToRadians(float degrees) {
    return degrees * 0.017453292519943295f;
}

std::vector<eyelib::math::Mat4> BuildBindPoseMatrices(const std::shared_ptr<eyelib::api::Skeleton>& skeleton) {
    if (!skeleton) {
        return {};
    }
    auto internal = skeleton->GetInternal();
    if (!internal || internal->BoneCount() == 0) {
        return {};
    }
    const size_t count = internal->BoneCount();
    std::vector<eyelib::math::Mat4> local(count, eyelib::math::Mat4(1.0f));
    for (size_t i = 0; i < count; ++i) {
        const auto& bone = internal->GetBone(i);
        local[i] = eyelib::math::Compose(bone.bindPosition, bone.bindRotation, bone.bindScale);
    }

    std::vector<eyelib::math::Mat4> global(count, eyelib::math::Mat4(1.0f));
    std::vector<uint8_t> computed(count, 0);
    std::function<void(size_t)> computeGlobal = [&](size_t idx) {
        if (computed[idx]) return;
        const auto& bone = internal->GetBone(idx);
        if (bone.parentIndex >= 0) {
            const size_t parent = static_cast<size_t>(bone.parentIndex);
            if (parent < count) {
                computeGlobal(parent);
                global[idx] = global[parent] * local[idx];
            } else {
                global[idx] = local[idx];
            }
        } else {
            global[idx] = local[idx];
        }
        computed[idx] = 1;
    };

    std::vector<eyelib::math::Mat4> out(count, eyelib::math::Mat4(1.0f));
    for (size_t i = 0; i < count; ++i) {
        computeGlobal(i);
        const auto& bone = internal->GetBone(i);
        out[i] = internal->GetGlobalInverseTransform() * global[i] * bone.inverseBindMatrix;
    }
    return out;
}

eyelib::math::Mat4 TransformToMatrix(const eyelib::world::components::Transform& transform) {
    return eyelib::math::Compose(transform.position, transform.rotation, transform.scale);
}

eyelib::math::Vec3 ForwardFromRotation(const eyelib::math::Quat& rotation) {
    const eyelib::math::Vec3 forward(0.0f, 0.0f, -1.0f);
    return rotation * forward;
}

} // namespace

bool WorldRenderBridge::Initialize(uint32_t width, uint32_t height) {
    if (!m_renderer.Initialize()) {
        return false;
    }
    if (!m_pipeline.Initialize(width, height)) {
        m_renderer.Shutdown();
        return false;
    }
    m_pipeline.SetMode(eyelib::api::RenderPipeline::Mode::Forward);
    m_width = width;
    m_height = height;
    m_ready = true;
    return true;
}

void WorldRenderBridge::Shutdown() {
    if (!m_ready) {
        return;
    }
    m_pipeline.Shutdown();
    m_renderer.Shutdown();
    m_modelCache.clear();
    m_skinnedMaterials.clear();
    m_defaultMaterial.reset();
    m_defaultShader.reset();
    m_skinnedShader.reset();
    m_ready = false;
}

void WorldRenderBridge::EnsureDefaultMaterial() {
    if (m_defaultMaterial) {
        return;
    }
    auto shader = std::make_shared<eyelib::api::Shader>();
    if (shader->LoadFromFile("assets/shaders/passes/forward/basic.vert",
                             "assets/shaders/passes/forward/basic.frag")) {
        auto mat = std::make_shared<eyelib::api::Material>();
        mat->SetShader(shader);
        m_defaultShader = shader;
        m_defaultMaterial = mat;
        return;
    }

    eyelib::api::core::Log::Warn("Default shader missing; using empty material.");
    m_defaultMaterial = std::make_shared<eyelib::api::Material>();
}

void WorldRenderBridge::EnsureSkinnedMaterial() {
    if (m_skinnedShader) {
        return;
    }
    auto shader = std::make_shared<eyelib::api::Shader>();
    if (shader->LoadFromFile("assets/shaders/passes/forward/skinned.vert",
                             "assets/shaders/passes/forward/skinned.frag")) {
        m_skinnedShader = shader;
        return;
    }
    eyelib::api::core::Log::Warn("Skinned shader missing; falling back to default material.");
}

std::shared_ptr<eyelib::api::MaterialInstance> WorldRenderBridge::GetSkinnedMaterialInstance(
    ::eyelib::world::EntityId entity,
    const std::shared_ptr<eyelib::api::Material>& baseMaterial) {
    for (const auto& entry : m_skinnedMaterials) {
        if (entry.entity == entity) {
            return entry.instance;
        }
    }

    std::shared_ptr<eyelib::api::Material> resolvedBase = baseMaterial;
    if (!resolvedBase) {
        EnsureSkinnedMaterial();
        auto material = std::make_shared<eyelib::api::Material>();
        if (m_skinnedShader) {
            material->SetShader(m_skinnedShader);
        } else {
            EnsureDefaultMaterial();
            material = m_defaultMaterial;
        }
        resolvedBase = material;
    }

    auto instance = std::make_shared<eyelib::api::MaterialInstance>(resolvedBase);
    m_skinnedMaterials.push_back({ entity, resolvedBase, instance });
    return instance;
}

WorldRenderBridge::ModelEntry WorldRenderBridge::LoadModelEntry(const std::string& path) {
    auto it = m_modelCache.find(path);
    if (it != m_modelCache.end()) {
        return it->second;
    }

    ModelEntry entry{};
    std::shared_ptr<eyelib::api::ModelAsset> model;
    try {
        model = eyelib::api::ModelLoader::LoadModel(path);
    } catch (const std::exception& e) {
        eyelib::api::core::Log::ErrorF("Failed to load model: %s - %s", path.c_str(), e.what());
        m_modelCache.emplace(path, entry);
        return entry;
    } catch (...) {
        eyelib::api::core::Log::ErrorF("Failed to load model: %s (unknown exception)", path.c_str());
        m_modelCache.emplace(path, entry);
        return entry;
    }
    if (!model) {
        eyelib::api::core::Log::WarnF("Failed to load model: %s", path.c_str());
        m_modelCache.emplace(path, entry);
        return entry;
    }

    const auto& meshes = model->GetMeshes();
    const auto& materials = model->GetMaterials();
    if (!meshes.empty()) {
        entry.mesh = meshes.front();
    }
    if (!materials.empty()) {
        entry.material = materials.front();
    }
    entry.skeleton = model->GetSkeleton();
    entry.bindPoseMatrices = BuildBindPoseMatrices(entry.skeleton);

    if (!entry.material) {
        EnsureDefaultMaterial();
        entry.material = m_defaultMaterial;
    }

    m_modelCache.emplace(path, entry);
    return entry;
}

std::shared_ptr<eyelib::api::Material> WorldRenderBridge::ResolveMaterial(
    const std::string& materialId,
    eyelib::api::AssetRegistry* registry) {
    if (materialId.empty() || !registry) {
        return nullptr;
    }
    auto id = eyelib::api::AssetId::FromString(materialId);
    if (id.IsValid()) {
        auto mat = registry->GetMaterial(id);
        if (mat) {
            return mat;
        }
    }
    return registry->GetMaterial(materialId);
}

void WorldRenderBridge::BuildScene(const eyelib::api::world::World& world,
                                   const RuntimeOptions& options,
                                   const WorldAnimationBridge* animationBridge,
                                   eyelib::api::AssetRegistry* assetRegistry,
                                   eyelib::api::RenderScene& scene) {
    (void)options;
    scene.opaque.clear();
    scene.transparent.clear();
    scene.pointLights.clear();
    scene.spotLights.clear();

    const auto obs = world.GetObservation();
    const auto transforms = obs.GetTransformsView();
    auto findTransform = [&transforms](eyelib::world::EntityId entity) -> const eyelib::world::components::Transform* {
        for (const auto& entry : transforms) {
            if (entry.entity == entity) {
                return &entry.component;
            }
        }
        return nullptr;
    };

    const auto renderables = obs.GetRenderablesView();
    for (const auto& entry : renderables) {
        const auto& renderable = entry.component;
        if (!renderable.visible) {
            continue;
        }

        std::string meshId = renderable.meshAssetId;
        if (!renderable.lodMeshAssetIds.empty() && renderable.activeLOD < renderable.lodMeshAssetIds.size()) {
            meshId = renderable.lodMeshAssetIds[renderable.activeLOD];
        }
        const std::string meshResolved = ResolveAssetPath(meshId, options.assetRoots);
        if (meshResolved.empty()) {
            continue;
        }

        auto modelEntry = LoadModelEntry(meshResolved);
        if (!modelEntry.mesh) {
            continue;
        }

        eyelib::api::Drawable drawable;
        drawable.mesh = modelEntry.mesh;
        drawable.material = modelEntry.material;
        if (!renderable.materialAssetId.empty()) {
            const std::string matResolved = ResolveAssetPath(renderable.materialAssetId, options.assetRoots);
            if (!matResolved.empty() && assetRegistry) {
                if (auto material = assetRegistry->GetMaterial(matResolved)) {
                    drawable.material = material;
                }
            }
        }
        if (const auto* transform = findTransform(entry.entity)) {
            drawable.transform = TransformToMatrix(*transform);
        }
        if (drawable.material) {
            const std::vector<eyelib::math::Mat4>* bones = nullptr;
            if (animationBridge) {
                bones = animationBridge->GetBoneMatrices(entry.entity);
                if (bones && bones->empty()) {
                    bones = nullptr;
                }
            }
            if (!bones && !modelEntry.bindPoseMatrices.empty()) {
                bones = &modelEntry.bindPoseMatrices;
            }
            if (bones) {
                drawable.materialInstance = GetSkinnedMaterialInstance(entry.entity, drawable.material);
                drawable.materialInstance->SetMat4ArrayOverride("u_Bones", *bones);
            }
        }
        scene.opaque.push_back(drawable);
    }

    const auto meshRenderers = obs.GetMeshRenderersView();
    for (const auto& entry : meshRenderers) {
        const auto& renderer = entry.component;
        if (!renderer.visible || renderer.meshAssetId.empty()) {
            continue;
        }
        const std::string meshResolved = ResolveAssetPath(renderer.meshAssetId, options.assetRoots);
        if (meshResolved.empty()) {
            continue;
        }
        auto modelEntry = LoadModelEntry(meshResolved);
        if (!modelEntry.mesh) {
            continue;
        }
        eyelib::api::Drawable drawable;
        drawable.mesh = modelEntry.mesh;
        drawable.material = modelEntry.material;
        if (!renderer.materialAssetId.empty()) {
            const std::string matResolved = ResolveAssetPath(renderer.materialAssetId, options.assetRoots);
            if (!matResolved.empty() && assetRegistry) {
                if (auto material = assetRegistry->GetMaterial(matResolved)) {
                    drawable.material = material;
                }
            }
        }
        if (const auto* transform = findTransform(entry.entity)) {
            drawable.transform = TransformToMatrix(*transform);
        }
        if (drawable.material) {
            const std::vector<eyelib::math::Mat4>* bones = nullptr;
            if (animationBridge) {
                bones = animationBridge->GetBoneMatrices(entry.entity);
                if (bones && bones->empty()) {
                    bones = nullptr;
                }
            }
            if (!bones && !modelEntry.bindPoseMatrices.empty()) {
                bones = &modelEntry.bindPoseMatrices;
            }
            if (bones) {
                drawable.materialInstance = GetSkinnedMaterialInstance(entry.entity, drawable.material);
                drawable.materialInstance->SetMat4ArrayOverride("u_Bones", *bones);
            }
        }
        scene.opaque.push_back(drawable);
    }

    const auto cameras = obs.GetCamerasView();
    bool cameraFound = false;
    eyelib::world::components::Camera cameraComponent{};
    eyelib::world::components::Transform cameraTransform{};
    for (const auto& entry : cameras) {
        if (!cameraFound || entry.component.isPrimary) {
            cameraFound = true;
            cameraComponent = entry.component;
            if (const auto* transform = findTransform(entry.entity)) {
                cameraTransform = *transform;
            }
            if (entry.component.isPrimary) {
                break;
            }
        }
    }

    if (cameraFound) {
        const float aspect = (cameraComponent.aspect > 0.0f)
            ? cameraComponent.aspect
            : (m_height > 0 ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f);
        if (cameraComponent.isPerspective) {
            scene.camera.projection = eyelib::math::Perspective(
                DegreesToRadians(cameraComponent.fov),
                aspect,
                cameraComponent.nearPlane,
                cameraComponent.farPlane
            );
        } else {
            const float half = cameraComponent.orthoSize * 0.5f;
            scene.camera.projection = eyelib::math::Orthographic(
                -half * aspect,
                half * aspect,
                -half,
                half,
                cameraComponent.nearPlane,
                cameraComponent.farPlane
            );
        }

        const auto worldMatrix = TransformToMatrix(cameraTransform);
        scene.camera.view = eyelib::math::Inverse(worldMatrix);
        scene.camera.viewPos = cameraTransform.position;
        scene.camera.nearPlane = cameraComponent.nearPlane;
        scene.camera.farPlane = cameraComponent.farPlane;
    } else {
        const float aspect = (m_height > 0) ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
        scene.camera.projection = eyelib::math::Perspective(DegreesToRadians(60.0f), aspect, 0.1f, 1000.0f);
        scene.camera.view = eyelib::math::LookAt({0.0f, 2.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        scene.camera.viewPos = {0.0f, 2.0f, 5.0f};
    }

    const auto lights = obs.GetLightsView();
    bool directionalSet = false;
    for (const auto& entry : lights) {
        const auto& light = entry.component;
        const auto* transform = findTransform(entry.entity);

        if (light.type == eyelib::world::components::Light::Type::Directional && !directionalSet) {
            auto direction = eyelib::math::Vec3(0.5f, -1.0f, 0.2f);
            if (transform) {
                direction = ForwardFromRotation(transform->rotation);
            }
            scene.light.direction = glm::normalize(direction);
            scene.light.color = light.color;
            scene.light.intensity = light.intensity;
            scene.light.castShadows = light.castShadows;
            directionalSet = true;
        } else if (light.type == eyelib::world::components::Light::Type::Point) {
            eyelib::api::render::PointLight point{};
            if (transform) {
                point.position = transform->position;
            }
            point.color = light.color;
            point.intensity = light.intensity;
            point.radius = light.range;
            scene.pointLights.push_back(point);
        } else if (light.type == eyelib::world::components::Light::Type::Spot) {
            eyelib::api::render::SpotLight spot{};
            if (transform) {
                spot.position = transform->position;
                spot.direction = glm::normalize(ForwardFromRotation(transform->rotation));
            }
            spot.color = light.color;
            spot.intensity = light.intensity;
            spot.range = light.range;
            spot.innerConeCos = std::cos(DegreesToRadians(light.innerConeAngle));
            spot.outerConeCos = std::cos(DegreesToRadians(light.outerConeAngle));
            scene.spotLights.push_back(spot);
        }
    }
}

void WorldRenderBridge::Render(const RuntimeOptions& options, const eyelib::api::world::World& world,
                               const WorldAnimationBridge* animationBridge,
                               eyelib::api::AssetRegistry* assetRegistry,
                               uint32_t width, uint32_t height) {
    if (!m_ready) {
        return;
    }

    if (width != m_width || height != m_height) {
        m_width = width;
        m_height = height;
        m_pipeline.Resize(width, height);
    }

    eyelib::api::RenderScene scene;
    BuildScene(world, options, animationBridge, assetRegistry, scene);
    m_pipeline.Render(scene);
    m_pipeline.BlitToScreen();
}

} // namespace openeye::player
