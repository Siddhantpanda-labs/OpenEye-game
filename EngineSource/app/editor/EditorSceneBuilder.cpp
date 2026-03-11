#include "EditorSceneBuilder.h"

#include "api/asset/AssetRegistry.h"
#include "api/asset/AssetId.h"
#include "api/asset/AssetTypes.h"
#include "api/geometry/BuiltinMeshIds.h"
#include "api/geometry/Primitives.h"
#include "api/render/Light.h"
#include "api/world/All.h"
#include "api/asset/loaders/ModelLoader.h"
#include "api/core/All.h"
#include "api/math/TransformUtils.h"
#include "api/platform/FileSystem.h"
#include "api/render/Material.h"
#include "api/render/RenderScene.h"
#include "api/render/Shader.h"
#include "api/render/TextureCube.h"
#include "api/asset/AssetManager.h"

#include <cmath>
#include <exception>

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
    auto shader = std::make_shared<eyelib::api::Shader>();
    if (m_renderDevice && shader->Load(m_renderDevice, "passes/forward/lit")) {
        auto mat = std::make_shared<eyelib::api::Material>();
        mat->SetShader(shader);
        m_defaultMaterial = mat;
        return;
    }
    if (!m_renderDevice) {
        eyelib::api::core::Log::Warn("EditorSceneBuilder: default shader not loaded — render device is null (passes/forward/lit). SetRenderDevice before Build.");
    } else {
        eyelib::api::core::Log::Warn("EditorSceneBuilder: default shader failed to load (passes/forward/lit). Check assets/shaders/opengl/ and runtime assets. Using empty material.");
    }
    m_defaultMaterial = std::make_shared<eyelib::api::Material>();
}

EditorSceneBuilder::ModelEntry EditorSceneBuilder::LoadModelEntry(eyelib::api::AssetRegistry* registry, const std::string& path) {
    auto it = m_modelCache.find(path);
    if (it != m_modelCache.end()) return it->second;

    ModelEntry entry{};
    try {
        std::shared_ptr<eyelib::api::ModelAsset> model = registry ? eyelib::api::ModelLoader::LoadModel(registry, path) : nullptr;
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

EditorSceneBuilder::ModelEntry EditorSceneBuilder::GetPrimitiveEntry(const std::string& meshId) {
    using namespace eyelib::api::geometry;
    if (!IsBuiltinMeshId(meshId)) return ModelEntry{};
    auto it = m_primitiveCache.find(meshId);
    if (it != m_primitiveCache.end()) return it->second;

    ModelEntry entry{};
    std::shared_ptr<eyelib::api::Mesh> mesh;
    if (meshId == BuiltinMesh::Cube) {
        mesh = eyelib::api::geometry::Primitives::CreateCube(0.5f);
    } else if (meshId == BuiltinMesh::Sphere) {
        mesh = eyelib::api::geometry::Primitives::CreateSphere(0.5f, 32, 16);
    } else if (meshId == BuiltinMesh::Plane) {
        mesh = eyelib::api::geometry::Primitives::CreatePlane(1.0f, 1.0f, 1, 1);
    } else if (meshId == BuiltinMesh::Cylinder) {
        mesh = eyelib::api::geometry::Primitives::CreateCylinder(0.5f, 1.0f, 32, 1, true, true);
    } else if (meshId == BuiltinMesh::Cone) {
        mesh = eyelib::api::geometry::Primitives::CreateCone(0.5f, 1.0f, 32, true);
    } else if (meshId == BuiltinMesh::Torus) {
        mesh = eyelib::api::geometry::Primitives::CreateTorus(0.5f, 0.2f, 32, 16);
    }
    if (mesh) {
        if (m_renderDevice)
            mesh->UploadToGPU(m_renderDevice);
        entry.mesh = std::move(mesh);
        EnsureDefaultMaterial();
        entry.material = m_defaultMaterial;
    }
    m_primitiveCache[meshId] = entry;
    return entry;
}

std::shared_ptr<eyelib::api::TextureCube> EditorSceneBuilder::LoadSkyboxFromAssetId(const std::string& assetId,
                                                                                    const std::vector<std::string>& assetRoots,
                                                                                    eyelib::api::RenderDevice device) {
    if (assetId.empty() || !device) return nullptr;
    const char* kFaces[] = { "posx.jpg", "negx.jpg", "posy.jpg", "negy.jpg", "posz.jpg", "negz.jpg" };
    std::string paths[6];
    for (int i = 0; i < 6; ++i) {
        std::string id = assetId + "/" + kFaces[i];
        paths[i] = ResolveAssetPath(id, assetRoots);
        if (paths[i].empty()) {
            eyelib::api::core::Log::WarnF("EditorSceneBuilder: skybox face missing: %s", id.c_str());
            return nullptr;
        }
    }
    auto skybox = std::make_shared<eyelib::api::TextureCube>();
    if (!skybox->LoadFromFiles(device, paths[0], paths[1], paths[2], paths[3], paths[4], paths[5], true, true)) {
        eyelib::api::core::Log::WarnF("EditorSceneBuilder: failed to load skybox %s", assetId.c_str());
        return nullptr;
    }
    return skybox;
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
    if (world) {
        const std::string skyboxId = world->GetSkyboxAssetId();
        if (!skyboxId.empty()) {
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
        ModelEntry modelEntry;
        if (eyelib::api::geometry::IsBuiltinMeshId(meshId)) {
            modelEntry = GetPrimitiveEntry(meshId);
        } else {
            const std::string meshResolved = ResolveAssetPath(meshId, assetRoots, assetRegistry);
            if (meshResolved.empty()) continue;
            modelEntry = LoadModelEntry(assetRegistry, meshResolved);
        }
        if (!modelEntry.mesh) continue;

        eyelib::api::Drawable drawable{};
        drawable.mesh = modelEntry.mesh;
        drawable.material = modelEntry.material;
        if (!renderable.materialAssetId.empty() && assetRegistry) {
            const std::string matResolved = ResolveAssetPath(renderable.materialAssetId, assetRoots, assetRegistry);
            if (!matResolved.empty()) {
                if (auto mat = ResolveMaterialForScene(assetRegistry, matResolved))
                    drawable.material = mat;
            }
        }
        if (const auto transform = getTransform(entry.entity))
            drawable.transform = TransformToMatrix(*transform);
        drawable.submeshIndex = renderable.submeshIndex;
        if (drawable.material)
            eyelib::api::FillDrawableRenderParams(drawable.params, drawable.material, nullptr);
        scene.opaqueStorage.push_back(drawable);
        if (outDrawableToEntity)
            outDrawableToEntity->push_back(entry.entity);
    }

    for (const auto& entry : snapshot.meshRenderers) {
        const auto& renderer = entry.component;
        if (!renderer.visible || renderer.meshAssetId.empty()) continue;
        const std::string& meshId = renderer.meshAssetId;
        ModelEntry modelEntry;
        if (eyelib::api::geometry::IsBuiltinMeshId(meshId)) {
            modelEntry = GetPrimitiveEntry(meshId);
        } else {
            const std::string meshResolved = ResolveAssetPath(meshId, assetRoots, assetRegistry);
            if (meshResolved.empty()) continue;
            modelEntry = LoadModelEntry(assetRegistry, meshResolved);
        }
        if (!modelEntry.mesh) continue;

        eyelib::api::Drawable drawable{};
        drawable.mesh = modelEntry.mesh;
        drawable.material = modelEntry.material;
        if (!renderer.materialAssetId.empty() && assetRegistry) {
            const std::string matResolved = ResolveAssetPath(renderer.materialAssetId, assetRoots, assetRegistry);
            if (!matResolved.empty()) {
                if (auto mat = ResolveMaterialForScene(assetRegistry, matResolved))
                    drawable.material = mat;
            }
        }
        if (const auto transform = getTransform(entry.entity))
            drawable.transform = TransformToMatrix(*transform);
        drawable.submeshIndex = renderer.submeshIndex;
        if (drawable.material)
            eyelib::api::FillDrawableRenderParams(drawable.params, drawable.material, nullptr);
        scene.opaqueStorage.push_back(drawable);
        if (outDrawableToEntity)
            outDrawableToEntity->push_back(entry.entity);
    }

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
