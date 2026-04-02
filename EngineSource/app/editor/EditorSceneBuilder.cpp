#include "EditorSceneBuilder.h"

#include "api/render/SkyboxCubemapLoad.h"
#include "api/asset/AssetRegistry.h"
#include "api/asset/AssetId.h"
#include "api/asset/AssetTypes.h"
#include "api/geometry/BuiltinMeshIds.h"
#include "api/geometry/Primitives.h"
#include "api/render/Light.h"
#include "api/world/All.h"
#include "api/core/All.h"
#include "api/math/TransformUtils.h"
#include "api/platform/FileSystem.h"
#include "api/render/Material.h"
#include "api/render/RenderScene.h"
#include "api/render/Shader.h"
#include "api/render/TextureCube.h"
#include "api/render/CameraConventions.h"
#include "api/asset/AssetManager.h"

#include <cmath>
#include <exception>
#include "eyelib/world/SkyboxAssetId.h"

#include <unordered_set>

namespace {
const float kPi = 3.14159265f;
}

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

/** World-space forward direction (-Z) from a transform matrix (column 2 negated). */
eyelib::math::Vec3 DirectionFromTransform(const eyelib::math::Mat4& m) {
    eyelib::math::Vec3 d(-m[2][0], -m[2][1], -m[2][2]);
    float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len > 1e-6f) {
        d.x /= len; d.y /= len; d.z /= len;
    } else {
        d = eyelib::math::Vec3(0.5f, -1.0f, 0.2f);
    }
    return d;
}

/** Editor free-fly camera: position + yaw/pitch -> view matrix. */
void EditorCameraToView(const EditorCamera& cam, uint32_t viewportWidth, uint32_t viewportHeight,
                       eyelib::api::CameraView& out) {
    eyelib::api::camera::FreeFlyCameraDesc cameraDesc;
    cameraDesc.position = eyelib::math::Vec3(cam.position[0], cam.position[1], cam.position[2]);
    cameraDesc.yaw = cam.yaw;
    cameraDesc.pitch = cam.pitch;
    cameraDesc.fovDegrees = cam.fovDegrees;
    cameraDesc.nearPlane = 0.1f;
    cameraDesc.farPlane = 1000.0f;
    out = eyelib::api::camera::BuildFreeFlyCameraView(
        cameraDesc,
        static_cast<float>(viewportWidth),
        static_cast<float>(viewportHeight));
}

} // namespace

void EditorSceneBuilder::BindSceneAssets(const eyelib::api::world::World& world,
                                         const std::vector<std::string>& assetRoots,
                                         eyelib::api::AssetRegistry& registry) {
    const auto refs = world.CollectAssetReferences();
    for (const auto& ref : refs) {
        if (ref.id.empty()) continue;
        if (ref.kind == eyelib::api::world::World::AssetRefKind::Script) continue;

        eyelib::api::AssetType type = eyelib::api::AssetType::Unknown;
        switch (ref.kind) {
            case eyelib::api::world::World::AssetRefKind::Material:
                type = eyelib::api::AssetType::Material;
                break;
            case eyelib::api::world::World::AssetRefKind::Mesh:
                type = eyelib::api::AssetType::Mesh;
                break;
            case eyelib::api::world::World::AssetRefKind::Animation:
            case eyelib::api::world::World::AssetRefKind::Skeleton:
                type = eyelib::api::AssetType::Animation;
                break;
            default:
                continue;
        }

        const std::string resolved = ResolveAssetPath(ref.id, assetRoots, &registry);
        if (resolved.empty()) continue;

        try {
            registry.LoadAsset(resolved, type);
        } catch (const std::exception& e) {
            eyelib::api::core::Log::ErrorF("EditorSceneBuilder::BindSceneAssets: failed to load %s - %s", resolved.c_str(), e.what());
        } catch (...) {
            eyelib::api::core::Log::ErrorF("EditorSceneBuilder::BindSceneAssets: failed to load %s", resolved.c_str());
        }
    }
}

std::string EditorSceneBuilder::ResolveAssetPath(const std::string& id, const std::vector<std::string>& roots, eyelib::api::AssetRegistry* registry) {
    if (id.empty()) return {};
    if (api::platform::filesystem::FileExists(id)) return id;
    for (const auto& root : roots) {
        const std::string candidate = JoinPath(root, id);
        if (api::platform::filesystem::FileExists(candidate)) return candidate;
    }
    if (registry) {
        eyelib::api::AssetId assetId = eyelib::api::AssetId::FromString(id);
        if (assetId.IsValid()) {
            std::string path = registry->GetFilePath(assetId);
            if (!path.empty()) return path;
        }
    }
    return {};
}

std::string EditorSceneBuilder::ResolveAssetPathCached(const std::string& id,
                                                       const std::vector<std::string>& roots,
                                                       eyelib::api::AssetRegistry* registry) {
    if (id.empty()) return {};
    auto it = m_resolvedAssetPathCache.find(id);
    if (it != m_resolvedAssetPathCache.end()) return it->second;
    std::string resolved = ResolveAssetPath(id, roots, registry);
    m_resolvedAssetPathCache.emplace(id, resolved);
    return resolved;
}

std::shared_ptr<eyelib::api::Material> EditorSceneBuilder::ResolveMaterialCached(eyelib::api::AssetRegistry* registry,
                                                                                  const std::string& pathOrId) {
    if (!registry || pathOrId.empty()) return nullptr;
    auto it = m_materialLookupCache.find(pathOrId);
    if (it != m_materialLookupCache.end()) {
        if (auto cached = it->second.lock()) return cached;
    }
    auto mat = ResolveMaterialForScene(registry, pathOrId);
    m_materialLookupCache[pathOrId] = mat;
    return mat;
}

std::shared_ptr<eyelib::api::Material> EditorSceneBuilder::ResolveMaterialForScene(eyelib::api::AssetRegistry* registry,
                                                                                     const std::string& pathOrId) {
    if (!registry || pathOrId.empty()) return nullptr;
    eyelib::api::AssetId apiId = eyelib::api::AssetId::FromString(pathOrId);
    if (!apiId.IsValid())
        apiId = registry->GetId(pathOrId);
    if (!apiId.IsValid()) return nullptr;
    return registry->GetMaterial(apiId);
}

void EditorSceneBuilder::EnsureDefaultMaterial() {
    if (m_defaultMaterial) return;
    auto mat = std::make_shared<eyelib::api::Material>();
    mat->SetMaterialShaderId(0);  // PBR; shader variant from ShaderCache at draw time
    mat->SetAlbedo(eyelib::math::Vec3(0.8f, 0.8f, 0.8f));
    m_defaultMaterial = mat;
}

EditorSceneBuilder::ModelEntry EditorSceneBuilder::LoadModelEntry(eyelib::api::AssetRegistry* registry, const std::string& path) {
    ModelEntry entry{};
    if (!registry) return entry;
    try {
        eyelib::api::AssetId meshId = registry->GetId(path);
        if (!meshId.IsValid())
            meshId = registry->LoadAsset(path, eyelib::api::AssetType::Mesh);
        if (!meshId.IsValid()) return entry;

        std::shared_ptr<eyelib::api::Mesh> apiMesh = registry->GetMesh(meshId);
        if (!apiMesh) return entry;
        entry.mesh = std::move(apiMesh);

        const std::vector<eyelib::api::AssetId> deps = registry->GetDependencies(meshId);
        for (const auto& depId : deps) {
            if (registry->GetAssetType(depId) == eyelib::api::AssetType::Material) {
                entry.material = registry->GetMaterial(depId);
                if (entry.material) break;
            }
        }
        if (!entry.material) {
            EnsureDefaultMaterial();
            entry.material = m_defaultMaterial;
        }
    } catch (const std::exception& e) {
        eyelib::api::core::Log::ErrorF("EditorSceneBuilder: failed to load model %s - %s", path.c_str(), e.what());
    } catch (...) {
        eyelib::api::core::Log::ErrorF("EditorSceneBuilder: failed to load model %s", path.c_str());
    }
    return entry;
}

EditorSceneBuilder::ModelEntry EditorSceneBuilder::GetPrimitiveEntry(const std::string& meshId) {
    using namespace eyelib::api::geometry;
    if (!IsBuiltinMeshId(meshId) || !m_renderDevice) return ModelEntry{};
    auto it = m_primitiveCache.find(meshId);
    if (it != m_primitiveCache.end()) return it->second;

    ModelEntry entry{};
    std::shared_ptr<eyelib::api::Mesh> mesh;
    if (meshId == BuiltinMesh::Cube) {
        mesh = Primitives::CreateCube(m_renderDevice, 0.5f);
    } else if (meshId == BuiltinMesh::Sphere) {
        mesh = Primitives::CreateSphere(m_renderDevice, 0.5f, 32, 16);
    } else if (meshId == BuiltinMesh::Plane) {
        mesh = Primitives::CreatePlane(m_renderDevice, 1.0f, 1.0f, 1, 1);
    } else if (meshId == BuiltinMesh::Cylinder) {
        mesh = Primitives::CreateCylinder(m_renderDevice, 0.5f, 1.0f, 32, 1, true, true);
    } else if (meshId == BuiltinMesh::Cone) {
        mesh = Primitives::CreateCone(m_renderDevice, 0.5f, 1.0f, 32, true);
    } else if (meshId == BuiltinMesh::Torus) {
        mesh = Primitives::CreateTorus(m_renderDevice, 0.5f, 0.2f, 32, 16);
    }
    if (mesh) {
        entry.mesh = std::move(mesh);
        EnsureDefaultMaterial();
        entry.material = m_defaultMaterial;
    }
    m_primitiveCache[meshId] = entry;
    return entry;
}

void EditorSceneBuilder::RebuildDrawableStaticIfNeeded(const DrawableCacheKey& key,
                                                       const std::string& meshId,
                                                       const std::string& materialAssetId,
                                                       int32_t submeshIndex,
                                                       const std::vector<std::string>& assetRoots,
                                                       eyelib::api::AssetRegistry* assetRegistry) {
    auto& cached = m_drawableStaticCache[key];
    cached.lastTouchedFrame = m_buildFrameCounter;

    const bool needsRebuild = !cached.valid
        || cached.meshId != meshId
        || cached.materialAssetId != materialAssetId
        || cached.submeshIndex != submeshIndex;
    if (!needsRebuild) return;

    cached.meshId = meshId;
    cached.materialAssetId = materialAssetId;
    cached.submeshIndex = submeshIndex;
    cached.mesh.reset();
    cached.material.reset();
    cached.valid = false;

    ModelEntry modelEntry;
    if (eyelib::api::geometry::IsBuiltinMeshId(meshId)) {
        modelEntry = GetPrimitiveEntry(meshId);
    } else {
        const std::string meshResolved = ResolveAssetPathCached(meshId, assetRoots, assetRegistry);
        if (meshResolved.empty()) return;
        modelEntry = LoadModelEntry(assetRegistry, meshResolved);
    }
    if (!modelEntry.mesh) return;

    cached.mesh = modelEntry.mesh;
    cached.material = modelEntry.material;
    if (!materialAssetId.empty() && assetRegistry) {
        if (auto mat = ResolveMaterialCached(assetRegistry, materialAssetId))
            cached.material = std::move(mat);
    }
    if (!cached.material) {
        EnsureDefaultMaterial();
        cached.material = m_defaultMaterial;
    }
    cached.valid = (cached.mesh != nullptr && cached.material != nullptr);
}

void EditorSceneBuilder::PruneStaleDrawableCache() {
    std::vector<DrawableCacheKey> stale;
    stale.reserve(m_drawableStaticCache.size());
    for (const auto& [key, value] : m_drawableStaticCache) {
        if (value.lastTouchedFrame != m_buildFrameCounter)
            stale.push_back(key);
    }
    for (const auto& key : stale)
        m_drawableStaticCache.erase(key);
}

std::shared_ptr<eyelib::api::TextureCube> EditorSceneBuilder::LoadSkyboxFromAssetId(const std::string& assetId,
                                                                                    const std::vector<std::string>& assetRoots,
                                                                                    eyelib::api::RenderDevice device) {
    if (assetId.empty() || !device || eyelib::world::skybox::IsProceduralSkyboxId(assetId))
        return nullptr;
    const auto resolve = [&](const std::string& id) { return ResolveAssetPath(id, assetRoots); };
    return eyelib::render::LoadSkyboxCubemapFromAssetId(device, assetId, resolve, eyelib::world::skybox::kDefaultCrossFaceSize);
}

void EditorSceneBuilder::Build(eyelib::api::RenderScene& scene,
                              const eyelib::api::world::World::RenderSnapshot& snapshot,
                              const EditorCamera& editorCamera,
                              uint32_t viewportWidth,
                              uint32_t viewportHeight,
                              const std::vector<std::string>& assetRoots,
                              eyelib::api::AssetRegistry* assetRegistry,
                              std::shared_ptr<eyelib::api::Material> defaultMaterial,
                              const eyelib::api::world::World* world,
                              std::vector<::eyelib::world::EntityId>* outDrawableToEntity) {
    m_renderDevice = assetRegistry ? assetRegistry->GetRenderDevice() : eyelib::api::RenderDevice();
    ++m_buildFrameCounter;
    std::string rootsSig;
    for (const auto& root : assetRoots) {
        rootsSig += root;
        rootsSig.push_back(';');
    }
    if (rootsSig != m_assetRootsSignature) {
        m_assetRootsSignature = std::move(rootsSig);
        m_resolvedAssetPathCache.clear();
        m_skyboxCache.clear();
        m_materialLookupCache.clear();
        m_drawableStaticCache.clear();
    }
    if (defaultMaterial) m_defaultMaterial = std::move(defaultMaterial);
    scene.opaqueStorage.clear();
    scene.transparentStorage.clear();
    scene.pointLightsStorage.clear();
    scene.spotLightsStorage.clear();
    if (outDrawableToEntity) outDrawableToEntity->clear();

    EditorCameraToView(editorCamera, viewportWidth, viewportHeight, scene.camera);

    scene.light.direction = eyelib::math::Vec3(0.5f, -1.0f, 0.2f);
    scene.light.color = eyelib::math::Vec3(1.0f, 0.95f, 0.9f);
    scene.light.intensity = 3.0f;
    scene.light.ambient = eyelib::math::Vec3(0.08f, 0.08f, 0.1f);
    scene.light.exposure = 1.0f;
    scene.light.castShadows = true;

    scene.skybox.reset();
    const std::string skyboxId = world ? world->GetSkyboxAssetId() : std::string();
    if (!eyelib::world::skybox::IsProceduralSkyboxId(skyboxId)) {
        auto it = m_skyboxCache.find(skyboxId);
        if (it != m_skyboxCache.end()) {
            scene.skybox = it->second;  // cached success or failure — no retry, no log
        } else {
            auto skybox = LoadSkyboxFromAssetId(skyboxId, assetRoots, m_renderDevice);
            if (skybox) {
                m_skyboxCache[skyboxId] = skybox;
                scene.skybox = std::move(skybox);
            } else {
                m_skyboxCache[skyboxId] = nullptr;  // cache failure so we don't retry every frame
            }
        }
    }

    auto getTransform = [&](::eyelib::world::EntityId id) { return snapshot.GetTransform(id); };

    bool directionalSet = false;
    for (const auto& lightEntry : snapshot.lights) {
        auto optTransform = getTransform(lightEntry.entity);
        if (!optTransform) continue;
        const auto& l = lightEntry.component;
        eyelib::math::Mat4 mat = TransformToMatrix(*optTransform);
        eyelib::math::Vec3 position(optTransform->position.x, optTransform->position.y, optTransform->position.z);
        eyelib::math::Vec3 direction = DirectionFromTransform(mat);

        using LT = ::eyelib::world::components::Light::Type;
        if (l.type == LT::Directional) {
            if (!directionalSet) {
                scene.light.direction = direction;
                scene.light.color = l.color;
                scene.light.intensity = l.intensity;
                scene.light.castShadows = l.castShadows;
                directionalSet = true;
            }
        } else if (l.type == LT::Point) {
            eyelib::api::render::PointLight pl;
            pl.position = position;
            pl.color = l.color;
            pl.intensity = l.intensity;
            pl.radius = l.range;
            scene.pointLightsStorage.push_back(pl);
        } else if (l.type == LT::Spot) {
            float innerRad = l.innerConeAngle * (kPi / 180.0f);
            float outerRad = l.outerConeAngle * (kPi / 180.0f);
            eyelib::api::render::SpotLight sl;
            sl.position = position;
            sl.direction = direction;
            sl.color = l.color;
            sl.intensity = l.intensity;
            sl.range = l.range;
            sl.innerConeCos = std::cos(innerRad);
            sl.outerConeCos = std::cos(outerRad);
            scene.spotLightsStorage.push_back(sl);
        }
    }

    for (const auto& entry : snapshot.renderables) {
        const auto& renderable = entry.component;
        if (!renderable.visible) continue;
        std::string meshId = renderable.meshAssetId;
        if (!renderable.lodMeshAssetIds.empty() && renderable.activeLOD < renderable.lodMeshAssetIds.size())
            meshId = renderable.lodMeshAssetIds[renderable.activeLOD];
        const DrawableCacheKey key{ entry.entity, 0u };
        RebuildDrawableStaticIfNeeded(key, meshId, renderable.materialAssetId, renderable.submeshIndex, assetRoots, assetRegistry);
        auto it = m_drawableStaticCache.find(key);
        if (it == m_drawableStaticCache.end() || !it->second.valid) continue;

        eyelib::api::Drawable drawable{};
        drawable.mesh = it->second.mesh;
        drawable.material = it->second.material;
        drawable.submeshIndex = it->second.submeshIndex;
        if (const auto transform = getTransform(entry.entity))
            drawable.transform = TransformToMatrix(*transform);
        if (drawable.material)
            eyelib::api::FillDrawableRenderParams(drawable.params, drawable.material, nullptr);
        scene.opaqueStorage.push_back(std::move(drawable));
        if (outDrawableToEntity)
            outDrawableToEntity->push_back(entry.entity);
    }

    for (const auto& entry : snapshot.meshRenderers) {
        const auto& renderer = entry.component;
        if (!renderer.visible || renderer.meshAssetId.empty()) continue;
        const DrawableCacheKey key{ entry.entity, 1u };
        RebuildDrawableStaticIfNeeded(key, renderer.meshAssetId, renderer.materialAssetId, renderer.submeshIndex, assetRoots, assetRegistry);
        auto it = m_drawableStaticCache.find(key);
        if (it == m_drawableStaticCache.end() || !it->second.valid) continue;

        eyelib::api::Drawable drawable{};
        drawable.mesh = it->second.mesh;
        drawable.material = it->second.material;
        drawable.submeshIndex = it->second.submeshIndex;
        if (const auto transform = getTransform(entry.entity))
            drawable.transform = TransformToMatrix(*transform);
        if (drawable.material)
            eyelib::api::FillDrawableRenderParams(drawable.params, drawable.material, nullptr);
        scene.opaqueStorage.push_back(std::move(drawable));
        if (outDrawableToEntity)
            outDrawableToEntity->push_back(entry.entity);
    }

    PruneStaleDrawableCache();

    scene.opaque = scene.opaqueStorage.empty() ? nullptr : scene.opaqueStorage.data();
    scene.opaqueCount = scene.opaqueStorage.size();
    scene.transparent = nullptr;
    scene.transparentCount = 0;
    scene.pointLights = scene.pointLightsStorage.empty() ? nullptr : scene.pointLightsStorage.data();
    scene.pointLightCount = scene.pointLightsStorage.size();
    scene.spotLights = scene.spotLightsStorage.empty() ? nullptr : scene.spotLightsStorage.data();
    scene.spotLightCount = scene.spotLightsStorage.size();
}

} // namespace openeye::editor
