#include "WorldRender.h"

#include "BuiltinResourceManager.h"
#include "RendererGPUResourceCache.h"
#include "api/asset/AssetId.h"
#include "api/asset/AssetRegistry.h"
#include "api/animation/Skeleton.h"
#include "api/core/All.h"
#include "api/geometry/BuiltinMeshIds.h"
#include "api/math/TransformUtils.h"
#include "api/platform/All.h"
#include "api/render/Material.h"
#include "api/render/RenderScene.h"
#include "api/render/RenderTarget.h"
#include "WorldAnimation.h"
#include "api/asset/AssetManager.h"
#include "eyelib/vr/XRViewHelpers.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <exception>
#include <optional>
#include <glm/glm.hpp>

namespace openeye::player {

namespace {

static std::string JoinPath(const std::string& base, const std::string& leaf) {
    if (base.empty()) return leaf;
    const char last = base.back();
    if (last == '/' || last == '\\') return base + leaf;
    return base + "/" + leaf;
}

static eyelib::api::RenderPipeline::Mode SceneModeToPipelineMode(const std::string& mode) {
    if (mode == "Deferred") return eyelib::api::RenderPipeline::Mode::Deferred;
    if (mode == "ForwardPlus") return eyelib::api::RenderPipeline::Mode::ForwardPlus;
    return eyelib::api::RenderPipeline::Mode::Forward;
}

static std::string ResolveAssetPath(const std::string& id, const std::vector<std::string>& roots,
                                    eyelib::api::AssetRegistry* registry = nullptr) {
    using eyelib::api::platform::filesystem::FileExists;
    if (id.empty()) return {};
    if (FileExists(id)) return id;
    for (const auto& root : roots) {
        const std::string candidate = JoinPath(root, id);
        if (FileExists(candidate)) return candidate;
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

bool WorldRenderBridge::Initialize(eyelib::api::Window& window, uint32_t width, uint32_t height,
                                   eyelib::api::AssetRegistry* assetRegistry,
                                   eyelib::render::RenderBackend backend) {
    if (!m_renderer.Initialize()) {
        return false;
    }
    if (!m_pipeline.Initialize(window, width, height, backend)) {
        m_renderer.Shutdown();
        return false;
    }
    m_renderer.SetDevice(m_pipeline.GetDevice());
    m_pipeline.SetMode(eyelib::api::RenderPipeline::Mode::Forward);
    m_width = width;
    m_height = height;
    m_builtinManager = std::make_unique<BuiltinResourceManager>();
    m_builtinManager->SetDevice(m_pipeline.GetDevice());
    if (assetRegistry) {
        m_assetRegistryForResolver = assetRegistry;
        m_gpuResourceCache = std::make_unique<RendererGPUResourceCache>(m_pipeline.GetDevice().Get(), assetRegistry);
        assetRegistry->SetGPUResolver(m_gpuResourceCache.get());
    }
    m_ready = true;
    return true;
}

bool WorldRenderBridge::Initialize(void* sharedDevice, uint32_t width, uint32_t height, eyelib::api::AssetRegistry* assetRegistry) {
    if (!sharedDevice) {
        return false;
    }
    if (!m_renderer.Initialize()) {
        return false;
    }
    if (!m_pipeline.InitializeWithSharedDevice(eyelib::api::RenderDevice(sharedDevice), width, height)) {
        m_renderer.Shutdown();
        return false;
    }
    m_renderer.SetDevice(m_pipeline.GetDevice());
    m_pipeline.SetMode(eyelib::api::RenderPipeline::Mode::Forward);
    m_width = width;
    m_height = height;
    m_builtinManager = std::make_unique<BuiltinResourceManager>();
    m_builtinManager->SetDevice(m_pipeline.GetDevice());
    if (assetRegistry) {
        m_assetRegistryForResolver = assetRegistry;
        m_gpuResourceCache = std::make_unique<RendererGPUResourceCache>(m_pipeline.GetDevice().Get(), assetRegistry);
        assetRegistry->SetGPUResolver(m_gpuResourceCache.get());
    }
    m_ready = true;
    return true;
}

void WorldRenderBridge::Shutdown() {
    if (!m_ready) {
        return;
    }
    for (int i = 0; i < kStreamingPBOCount; ++i) {
        if (m_streamingPBO[i]) { m_pipeline.DestroyPixelPackBuffer(m_streamingPBO[i]); m_streamingPBO[i] = nullptr; }
        if (m_streamingNV12PBO[i]) { m_pipeline.DestroyPixelPackBuffer(m_streamingNV12PBO[i]); m_streamingNV12PBO[i] = nullptr; }
    }
    for (int i = 0; i < kStreamingRTCount; ++i)
        m_streamingTargets[i].reset();
    m_streamingRTWriteIndex = 0;
    m_streamingRTFrameCount = 0;
    m_streamingPBOIndex = 0;
    m_streamingReadbackCount = 0;
    m_streamingNV12PBOIndex = 0;
    m_streamingNV12ReadbackCount = 0;
    // Destroy caches and shaders/materials first; their destructors call into the RHI device.
    // Only then shut down the pipeline (which destroys the device).
    if (m_assetRegistryForResolver)
        m_assetRegistryForResolver->SetGPUResolver(nullptr);
    m_assetRegistryForResolver = nullptr;
    m_gpuResourceCache.reset();
    m_builtinManager.reset();
    m_skyboxCache.clear();
    m_skinnedMaterials.clear();
    m_defaultMaterial.reset();
    m_defaultShader.reset();
    m_skinnedShader.reset();
    m_pipeline.Shutdown();
    m_renderer.Shutdown();
    m_ready = false;
}

void* WorldRenderBridge::GetOutputColorTextureNativeHandle() const {
    return m_ready ? m_pipeline.GetOutputColorTextureNativeHandle() : nullptr;
}

bool WorldRenderBridge::ReadOutputPixels(uint32_t width, uint32_t height, uint8_t* outRGBA) {
    if (!m_ready || !outRGBA || width == 0 || height == 0) return false;
    m_pipeline.Finish();
    return m_pipeline.ReadOutputPixels(width, height, outRGBA);
}

void WorldRenderBridge::PrepareStreamingFrame() {
    if (!m_ready) return;
    if (!m_streamingTargets[0]) {
        bool ok = true;
        for (int i = 0; i < kStreamingRTCount; ++i) {
            m_streamingTargets[i] = std::make_unique<eyelib::api::RenderTarget>();
            if (!m_streamingTargets[i]->Create(&m_pipeline, kStreamingWidth, kStreamingHeight,
                                               {eyelib::api::TextureFormat::RGBA8},
                                               eyelib::api::TextureFormat::Depth24, 0)) {
                ok = false;
                break;
            }
        }
        if (!ok) {
            for (int i = 0; i < kStreamingRTCount; ++i)
                m_streamingTargets[i].reset();
            return;
        }
    }
    auto& writeTarget = m_streamingTargets[m_streamingRTWriteIndex];
    if (writeTarget) {
        m_pipeline.BlitOutputTo(*writeTarget);
        if (m_streamingRTFrameCount < kStreamingRTCount)
            ++m_streamingRTFrameCount;
        const size_t pboSize = static_cast<size_t>(kStreamingWidth) * kStreamingHeight * 4u;
        if (!m_streamingPBO[0]) {
            for (int i = 0; i < kStreamingPBOCount; ++i)
                m_streamingPBO[i] = m_pipeline.CreatePixelPackBuffer(pboSize);
            bool ok = true;
            for (int i = 0; i < kStreamingPBOCount; ++i)
                if (!m_streamingPBO[i]) ok = false;
            if (!ok) {
                for (int i = 0; i < kStreamingPBOCount; ++i) {
                    if (m_streamingPBO[i]) { m_pipeline.DestroyPixelPackBuffer(m_streamingPBO[i]); m_streamingPBO[i] = nullptr; }
                }
            }
        }
        if (!m_streamingNV12PBO[0]) {
            for (int i = 0; i < kStreamingPBOCount; ++i)
                m_streamingNV12PBO[i] = m_pipeline.CreatePixelPackBuffer(kStreamingNV12Size);
            bool ok = true;
            for (int i = 0; i < kStreamingPBOCount; ++i)
                if (!m_streamingNV12PBO[i]) ok = false;
            if (!ok) {
                for (int i = 0; i < kStreamingPBOCount; ++i) {
                    if (m_streamingNV12PBO[i]) { m_pipeline.DestroyPixelPackBuffer(m_streamingNV12PBO[i]); m_streamingNV12PBO[i] = nullptr; }
                }
            }
        }
        if (m_streamingNV12PBO[0]) {
            m_pipeline.RunStreamingNV12Pass(*writeTarget, m_streamingNV12PBO[m_streamingNV12PBOIndex], kStreamingNV12Size, kStreamingWidth, kStreamingHeight, kStreamingPitch);
            m_streamingNV12PBOIndex = (m_streamingNV12PBOIndex + 1) % kStreamingPBOCount;
            if (m_streamingNV12ReadbackCount < kStreamingPBOCount) ++m_streamingNV12ReadbackCount;
        }
    }
}

bool WorldRenderBridge::ReadStreamingPixels(uint8_t* outRGBA) {
    if (!m_ready || !outRGBA || !m_streamingTargets[0]) return false;
    const int latestIdx = (m_streamingRTWriteIndex + kStreamingRTCount - 1) % kStreamingRTCount;
    if (!m_streamingTargets[latestIdx]) return false;
    m_pipeline.Finish();
    return m_pipeline.ReadPixelsFromRenderTarget(*m_streamingTargets[latestIdx], outRGBA);
}

bool WorldRenderBridge::GetStreamingPixelsFromPreviousFrame(uint8_t* outRGBA) {
    if (!m_ready || !outRGBA || !m_streamingPBO[0]) return false;
    if (m_streamingReadbackCount < 2) return false;
    const int prevIdx = (m_streamingPBOIndex + 2) % kStreamingPBOCount;
    const size_t size = static_cast<size_t>(kStreamingWidth) * kStreamingHeight * 4u;
    m_pipeline.MapPixelPackBuffer(m_streamingPBO[prevIdx], outRGBA, size, kStreamingWidth, kStreamingHeight);
    return true;
}

bool WorldRenderBridge::GetStreamingNV12FromPreviousFrame(uint8_t* outBuffer, size_t outSizeBytes) {
    if (!m_ready || !outBuffer || !m_streamingNV12PBO[0]) return false;
    if (outSizeBytes < kStreamingNV12Size) return false;
    if (m_streamingNV12ReadbackCount < 2) return false;
    const int prevIdx = (m_streamingNV12PBOIndex + 2) % kStreamingPBOCount;
    m_pipeline.MapPixelPackBufferRaw(m_streamingNV12PBO[prevIdx], outBuffer, kStreamingNV12Size);
    return true;
}

void WorldRenderBridge::IssueStreamingReadback() {
    if (!m_streamingTargets[0]) return;
    if (m_streamingPBO[0] && m_streamingRTFrameCount >= 2) {
        const int readRTIdx = (m_streamingRTWriteIndex + kStreamingRTCount - 1) % kStreamingRTCount;
        if (m_streamingTargets[readRTIdx]) {
            m_pipeline.ReadPixelsToPBO(*m_streamingTargets[readRTIdx], m_streamingPBO[m_streamingPBOIndex]);
            m_streamingPBOIndex = (m_streamingPBOIndex + 1) % kStreamingPBOCount;
            if (m_streamingReadbackCount < kStreamingPBOCount) ++m_streamingReadbackCount;
        }
    }
    m_streamingRTWriteIndex = (m_streamingRTWriteIndex + 1) % kStreamingRTCount;
}

eyelib::api::RenderDevice WorldRenderBridge::GetDevice() const {
    return m_pipeline.GetDevice();
}

void WorldRenderBridge::InitializeDefaultResources() {
    if (!m_pipeline.GetDevice()) return;
    if (m_builtinManager) m_builtinManager->InitializeDefaultResources();
    EnsureDefaultMaterial();
    EnsureSkinnedMaterial();
}

void WorldRenderBridge::ReloadShaders() {
    m_pipeline.ReloadShaders();
}

void WorldRenderBridge::ApplySceneRenderPipelineMode(const std::string& mode) {
    m_pipeline.SetMode(SceneModeToPipelineMode(mode));
}

void WorldRenderBridge::EnsureDefaultMaterial() {
    if (m_defaultMaterial) return;
    if (m_builtinManager) {
        m_defaultMaterial = m_builtinManager->GetDefaultMaterial();
        return;
    }
    auto mat = std::make_shared<eyelib::api::Material>();
    mat->SetMaterialShaderId(0);  // PBR; variant from ShaderCache at draw time
    mat->SetAlbedo(eyelib::math::Vec3(1.0f, 1.0f, 1.0f));
    m_defaultMaterial = mat;
}

void WorldRenderBridge::EnsureSkinnedMaterial() {
    // Skinned variant from ShaderCache at draw time (same PBR material).
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
        EnsureDefaultMaterial();
        resolvedBase = m_defaultMaterial;
    }

    auto instance = std::make_shared<eyelib::api::MaterialInstance>(resolvedBase);
    m_skinnedMaterials.push_back({ entity, resolvedBase, instance });
    return instance;
}

WorldRenderBridge::ModelEntry WorldRenderBridge::LoadModelEntry(eyelib::api::AssetRegistry* registry, const std::string& path) {
    ModelEntry entry{};
    if (!registry) return entry;
    try {
        eyelib::api::AssetId meshId = registry->GetId(path);
        if (!meshId.IsValid())
            meshId = registry->LoadAsset(path, eyelib::api::AssetType::Mesh);
        if (!meshId.IsValid()) return entry;

        entry.mesh = registry->GetMesh(meshId);
        if (!entry.mesh) return entry;

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

        entry.skeleton = registry->GetSkeleton(meshId);
        entry.bindPoseMatrices = BuildBindPoseMatrices(entry.skeleton);
    } catch (const std::exception& e) {
        eyelib::api::core::Log::ErrorF("Failed to load model: %s - %s", path.c_str(), e.what());
    } catch (...) {
        eyelib::api::core::Log::ErrorF("Failed to load model: %s (unknown exception)", path.c_str());
    }
    return entry;
}

WorldRenderBridge::ModelEntry WorldRenderBridge::GetPrimitiveEntry(const std::string& meshId) {
    using namespace eyelib::api::geometry;
    if (!IsBuiltinMeshId(meshId)) return ModelEntry{};
    if (m_builtinManager) {
        auto pe = m_builtinManager->GetPrimitiveEntry(meshId);
        ModelEntry entry{};
        entry.mesh = pe.mesh;
        entry.material = pe.material;
        return entry;
    }
    return ModelEntry{};
}

std::shared_ptr<eyelib::api::Material> WorldRenderBridge::ResolveMaterial(eyelib::api::AssetId id) {
    if (!id.IsValid()) return nullptr;
    if (m_assetRegistryForResolver) {
        auto mat = m_assetRegistryForResolver->GetMaterial(id);
        return mat;
    }
    return nullptr;
}

std::shared_ptr<eyelib::api::Material> WorldRenderBridge::ResolveMaterial(
    const std::string& materialId,
    eyelib::api::AssetRegistry* registry) {
    if (materialId.empty() || !registry) return nullptr;
    eyelib::api::AssetId id = eyelib::api::AssetId::FromString(materialId);
    if (id.IsValid()) {
        auto mat = ResolveMaterial(id);
        if (mat) return mat;
    }
    id = registry->GetId(materialId);
    return id.IsValid() ? ResolveMaterial(id) : nullptr;
}

std::shared_ptr<eyelib::api::TextureCube> WorldRenderBridge::GetOrLoadSkybox(const std::string& assetId,
                                                                             const std::vector<std::string>& assetRoots) {
    if (assetId.empty()) return nullptr;
    auto it = m_skyboxCache.find(assetId);
    if (it != m_skyboxCache.end()) {
        return it->second;  // cached success or failure (nullptr) — no retry, no log
    }
    const char* kFaces[] = { "posx.jpg", "negx.jpg", "posy.jpg", "negy.jpg", "posz.jpg", "negz.jpg" };
    std::string paths[6];
    for (int i = 0; i < 6; ++i) {
        paths[i] = ResolveAssetPath(assetId + "/" + kFaces[i], assetRoots);
        if (paths[i].empty()) {
            eyelib::api::core::Log::WarnF("WorldRenderBridge: skybox face missing: %s/%s", assetId.c_str(), kFaces[i]);
            m_skyboxCache[assetId] = nullptr;
            return nullptr;
        }
    }
    eyelib::api::RenderDevice device = m_pipeline.GetDevice();
    if (!device) {
        m_skyboxCache[assetId] = nullptr;
        return nullptr;
    }
    auto skybox = std::make_shared<eyelib::api::TextureCube>();
    if (!skybox->LoadFromFiles(device, paths[0], paths[1], paths[2], paths[3], paths[4], paths[5], true, true)) {
        eyelib::api::core::Log::WarnF("WorldRenderBridge: failed to load skybox %s", assetId.c_str());
        m_skyboxCache[assetId] = nullptr;
        return nullptr;
    }
    m_skyboxCache[assetId] = skybox;
    return skybox;
}

void WorldRenderBridge::BuildScene(eyelib::api::world::World& world,
                                   const RuntimeOptions& options,
                                   const WorldAnimationBridge* animationBridge,
                                   eyelib::api::AssetRegistry* assetRegistry,
                                   eyelib::api::RenderScene& scene,
                                   api::FrameAllocator* frameAlloc) {
    if (!m_captureCallbackSet) {
        world.SetCaptureSnapshotCallback([this, &world]() {
            world.CaptureRenderSnapshot(m_frameSnapshot);
            world.SetFrameSnapshot(&m_frameSnapshot);
        });
        m_captureCallbackSet = true;
        world.CaptureRenderSnapshot(m_frameSnapshot);
        world.SetFrameSnapshot(&m_frameSnapshot);
    }
    BuildScene(m_frameSnapshot, options, animationBridge, assetRegistry, scene, frameAlloc);

    const std::string skyboxId = world.GetSkyboxAssetId();
    if (!skyboxId.empty()) {
        scene.skybox = GetOrLoadSkybox(skyboxId, options.assetRoots);
    } else {
        scene.skybox.reset();
    }
}

void WorldRenderBridge::BuildScene(const eyelib::api::world::Observation& obs,
                                   const RuntimeOptions& options,
                                   const WorldAnimationBridge* animationBridge,
                                   eyelib::api::AssetRegistry* assetRegistry,
                                   eyelib::api::RenderScene& scene,
                                   api::FrameAllocator* frameAlloc) {
    const auto renderables = obs.GetRenderablesView();
    const auto meshRenderers = obs.GetMeshRenderersView();
    const auto cameras = obs.GetCamerasView();
    const auto xrCameras = obs.GetXRCamerasView();
    const auto lights = obs.GetLightsView();
    auto getTransform = [&](eyelib::world::EntityId id) { return obs.GetTransform(id); };
    BuildSceneFromViews(renderables, meshRenderers, cameras, xrCameras, lights, getTransform, options, animationBridge, assetRegistry, scene, frameAlloc);
}

void WorldRenderBridge::BuildScene(const eyelib::api::world::World::RenderSnapshot& snapshot,
                                   const RuntimeOptions& options,
                                   const WorldAnimationBridge* animationBridge,
                                   eyelib::api::AssetRegistry* assetRegistry,
                                   eyelib::api::RenderScene& scene,
                                   api::FrameAllocator* frameAlloc) {
    BuildSceneFromViews(snapshot.renderables, snapshot.meshRenderers, snapshot.cameras, snapshot.xrCameras, snapshot.lights,
                        [&](eyelib::world::EntityId id) { return snapshot.GetTransform(id); },
                        options, animationBridge, assetRegistry, scene, frameAlloc);
}

void WorldRenderBridge::BuildSceneFromViews(
    std::span<const eyelib::api::world::World::RenderableEntry> renderables,
    std::span<const eyelib::api::world::World::MeshRendererEntry> meshRenderers,
    std::span<const eyelib::api::world::World::CameraEntry> cameras,
    std::span<const eyelib::api::world::World::XRCameraEntry> xrCameras,
    std::span<const eyelib::api::world::World::LightEntry> lights,
    std::function<std::optional<::eyelib::world::components::Transform>(::eyelib::world::EntityId)> getTransform,
    const RuntimeOptions& options,
    const WorldAnimationBridge* animationBridge,
    eyelib::api::AssetRegistry* assetRegistry,
    eyelib::api::RenderScene& scene,
    api::FrameAllocator* frameAlloc) {
    (void)options;
    // Count pass: opaque (renderables + mesh renderers), point lights, spot lights
    size_t opaqueCount = 0;
    for (const auto& entry : renderables) {
        const auto& renderable = entry.component;
        if (!renderable.visible) continue;
        std::string meshId = renderable.meshAssetId;
        if (!renderable.lodMeshAssetIds.empty() && renderable.activeLOD < renderable.lodMeshAssetIds.size()) {
            meshId = renderable.lodMeshAssetIds[renderable.activeLOD];
        }
        if (eyelib::api::geometry::IsBuiltinMeshId(meshId)) {
            if (GetPrimitiveEntry(meshId).mesh) opaqueCount++;
        } else {
            const std::string meshResolved = ResolveAssetPath(meshId, options.assetRoots, assetRegistry);
            if (meshResolved.empty()) continue;
            if (LoadModelEntry(assetRegistry, meshResolved).mesh) opaqueCount++;
        }
    }
    for (const auto& entry : meshRenderers) {
        const auto& renderer = entry.component;
        if (!renderer.visible || renderer.meshAssetId.empty()) continue;
        const std::string& meshId = renderer.meshAssetId;
        if (eyelib::api::geometry::IsBuiltinMeshId(meshId)) {
            if (GetPrimitiveEntry(meshId).mesh) opaqueCount++;
        } else {
            const std::string meshResolved = ResolveAssetPath(meshId, options.assetRoots, assetRegistry);
            if (meshResolved.empty()) continue;
            if (LoadModelEntry(assetRegistry, meshResolved).mesh) opaqueCount++;
        }
    }
    size_t pointLightCount = 0;
    size_t spotLightCount = 0;
    for (const auto& entry : lights) {
        const auto& light = entry.component;
        if (light.type == eyelib::world::components::Light::Type::Point) pointLightCount++;
        else if (light.type == eyelib::world::components::Light::Type::Spot) spotLightCount++;
    }

    eyelib::api::Drawable* opaquePtr = nullptr;
    eyelib::api::render::PointLight* pointLightsPtr = nullptr;
    eyelib::api::render::SpotLight* spotLightsPtr = nullptr;
    if (frameAlloc) {
        opaquePtr = frameAlloc->AllocateArray<eyelib::api::Drawable>(opaqueCount);
        pointLightsPtr = frameAlloc->AllocateArray<eyelib::api::render::PointLight>(pointLightCount);
        spotLightsPtr = frameAlloc->AllocateArray<eyelib::api::render::SpotLight>(spotLightCount);
        scene.opaque = opaquePtr;
        scene.opaqueCount = opaqueCount;
        scene.transparent = nullptr;
        scene.transparentCount = 0;
        scene.pointLights = pointLightsPtr;
        scene.pointLightCount = pointLightCount;
        scene.spotLights = spotLightsPtr;
        scene.spotLightCount = spotLightCount;
    } else {
        scene.opaqueStorage.clear();
        scene.opaqueStorage.reserve(opaqueCount);
        scene.pointLightsStorage.clear();
        scene.pointLightsStorage.reserve(pointLightCount);
        scene.spotLightsStorage.clear();
        scene.spotLightsStorage.reserve(spotLightCount);
    }

    size_t opaqueIdx = 0;
    for (const auto& entry : renderables) {
        const auto& renderable = entry.component;
        if (!renderable.visible) continue;
        std::string meshId = renderable.meshAssetId;
        if (!renderable.lodMeshAssetIds.empty() && renderable.activeLOD < renderable.lodMeshAssetIds.size()) {
            meshId = renderable.lodMeshAssetIds[renderable.activeLOD];
        }
        ModelEntry modelEntry;
        if (eyelib::api::geometry::IsBuiltinMeshId(meshId)) {
            modelEntry = GetPrimitiveEntry(meshId);
        } else {
            const std::string meshResolved = ResolveAssetPath(meshId, options.assetRoots, assetRegistry);
            if (meshResolved.empty()) continue;
            modelEntry = LoadModelEntry(assetRegistry, meshResolved);
        }
        if (!modelEntry.mesh) continue;

        eyelib::api::Drawable drawable;
        drawable.mesh = modelEntry.mesh;
        drawable.material = modelEntry.material;
        drawable.submeshIndex = renderable.submeshIndex;
        if (!renderable.materialAssetId.empty() && assetRegistry) {
            if (auto material = ResolveMaterial(renderable.materialAssetId, assetRegistry))
                drawable.material = material;
        }
        if (const auto transform = getTransform(entry.entity)) {
            drawable.transform = TransformToMatrix(*transform);
        }
        if (drawable.material) {
            const std::vector<eyelib::math::Mat4>* bones = nullptr;
            if (animationBridge) {
                bones = animationBridge->GetBoneMatrices(entry.entity);
                if (bones && bones->empty()) bones = nullptr;
            }
            if (!bones && !modelEntry.bindPoseMatrices.empty()) {
                bones = &modelEntry.bindPoseMatrices;
            }
            if (bones && frameAlloc) {
                const size_t count = bones->size();
                if (count > 0) {
                    auto* copy = frameAlloc->AllocateArray<eyelib::math::Mat4>(count);
                    if (copy) {
                        std::memcpy(copy, bones->data(), sizeof(eyelib::math::Mat4) * count);
                        drawable.bones = copy;
                        drawable.boneCount = count;
                    }
                }
            } else if (bones) {
                drawable.materialInstance = GetSkinnedMaterialInstance(entry.entity, drawable.material);
                drawable.materialInstance->SetMat4ArrayOverride("u_Bones", *bones);
            }
        }
        if (drawable.material) {
            eyelib::api::FillDrawableRenderParams(drawable.params, drawable.material, drawable.materialInstance);
        }
        if (frameAlloc) {
            new (opaquePtr + opaqueIdx) eyelib::api::Drawable(drawable);
            opaqueIdx++;
        } else {
            scene.opaqueStorage.push_back(drawable);
        }
    }

    for (const auto& entry : meshRenderers) {
        const auto& renderer = entry.component;
        if (!renderer.visible || renderer.meshAssetId.empty()) continue;
        const std::string& meshId = renderer.meshAssetId;
        ModelEntry modelEntry;
        if (eyelib::api::geometry::IsBuiltinMeshId(meshId)) {
            modelEntry = GetPrimitiveEntry(meshId);
        } else {
            const std::string meshResolved = ResolveAssetPath(meshId, options.assetRoots, assetRegistry);
            if (meshResolved.empty()) continue;
            modelEntry = LoadModelEntry(assetRegistry, meshResolved);
        }
        if (!modelEntry.mesh) continue;

        eyelib::api::Drawable drawable;
        drawable.mesh = modelEntry.mesh;
        drawable.material = modelEntry.material;
        drawable.submeshIndex = renderer.submeshIndex;
        if (!renderer.materialAssetId.empty() && assetRegistry) {
            if (auto material = ResolveMaterial(renderer.materialAssetId, assetRegistry))
                drawable.material = material;
        }
        if (const auto transform = getTransform(entry.entity)) {
            drawable.transform = TransformToMatrix(*transform);
        }
        if (drawable.material) {
            const std::vector<eyelib::math::Mat4>* bones = nullptr;
            if (animationBridge) {
                bones = animationBridge->GetBoneMatrices(entry.entity);
                if (bones && bones->empty()) bones = nullptr;
            }
            if (!bones && !modelEntry.bindPoseMatrices.empty()) {
                bones = &modelEntry.bindPoseMatrices;
            }
            if (bones && frameAlloc) {
                const size_t count = bones->size();
                if (count > 0) {
                    auto* copy = frameAlloc->AllocateArray<eyelib::math::Mat4>(count);
                    if (copy) {
                        std::memcpy(copy, bones->data(), sizeof(eyelib::math::Mat4) * count);
                        drawable.bones = copy;
                        drawable.boneCount = count;
                    }
                }
            } else if (bones) {
                drawable.materialInstance = GetSkinnedMaterialInstance(entry.entity, drawable.material);
                drawable.materialInstance->SetMat4ArrayOverride("u_Bones", *bones);
            }
        }
        if (drawable.material) {
            eyelib::api::FillDrawableRenderParams(drawable.params, drawable.material, drawable.materialInstance);
        }
        if (frameAlloc) {
            new (opaquePtr + opaqueIdx) eyelib::api::Drawable(drawable);
            opaqueIdx++;
        } else {
            scene.opaqueStorage.push_back(drawable);
        }
    }

    if (!frameAlloc) {
        scene.opaque = scene.opaqueStorage.empty() ? nullptr : scene.opaqueStorage.data();
        scene.opaqueCount = scene.opaqueStorage.size();
        scene.transparent = nullptr;
        scene.transparentCount = 0;
    }

    // Active view priority: XR camera (if present) -> primary regular camera -> any regular camera -> fallback
    bool cameraFound = false;
    eyelib::world::components::Camera cameraComponent{};
    eyelib::world::components::Transform cameraTransform{};

    if (!xrCameras.empty()) {
        // Use XR camera as active view (prefer isPrimary when multiple)
        size_t xrIdx = 0;
        for (size_t i = 0; i < xrCameras.size(); ++i) {
            if (xrCameras[i].component.isPrimary) {
                xrIdx = i;
                break;
            }
        }
        const auto& xrEntry = xrCameras[xrIdx];
        const auto xrTransform = getTransform(xrEntry.entity);
        if (xrTransform) {
            eyelib::vr::XRViewData leftData, rightData;
            eyelib::vr::StereoViewDataFromCenterPose(
                xrTransform->position, xrTransform->rotation,
                eyelib::vr::kDefaultStereoIPD,
                eyelib::vr::kDefaultStereoFovHalfX,
                eyelib::vr::kDefaultStereoFovHalfY,
                leftData, rightData);
            eyelib::vr::XRViewProjection xrProj = eyelib::vr::ViewProjectionFromXRView(
                leftData,
                xrEntry.component.nearPlane,
                xrEntry.component.farPlane);
            scene.camera.view = xrProj.view;
            scene.camera.projection = xrProj.projection;
            scene.camera.viewPos = xrProj.viewPos;
            scene.camera.nearPlane = xrProj.nearPlane;
            scene.camera.farPlane = xrProj.farPlane;
            cameraFound = true;
        }
    }

    if (!cameraFound) {
        for (const auto& entry : cameras) {
            if (!cameraFound || entry.component.isPrimary) {
                cameraFound = true;
                cameraComponent = entry.component;
                if (const auto transform = getTransform(entry.entity)) {
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
        }
    }

    if (!cameraFound) {
        const float aspect = (m_height > 0) ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
        scene.camera.projection = eyelib::math::Perspective(DegreesToRadians(60.0f), aspect, 0.1f, 1000.0f);
        scene.camera.view = eyelib::math::LookAt({0.0f, 2.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
        scene.camera.viewPos = {0.0f, 2.0f, 5.0f};
    }

    bool directionalSet = false;
    size_t pointIdx = 0;
    size_t spotIdx = 0;
    for (const auto& entry : lights) {
        const auto& light = entry.component;
        const auto transform = getTransform(entry.entity);

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
            if (frameAlloc && pointLightsPtr) {
                pointLightsPtr[pointIdx++] = point;
            } else {
                scene.pointLightsStorage.push_back(point);
            }
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
            if (frameAlloc && spotLightsPtr) {
                spotLightsPtr[spotIdx++] = spot;
            } else {
                scene.spotLightsStorage.push_back(spot);
            }
        }
    }
    if (!frameAlloc) {
        scene.pointLights = scene.pointLightsStorage.empty() ? nullptr : scene.pointLightsStorage.data();
        scene.pointLightCount = scene.pointLightsStorage.size();
        scene.spotLights = scene.spotLightsStorage.empty() ? nullptr : scene.spotLightsStorage.data();
        scene.spotLightCount = scene.spotLightsStorage.size();
    }
}

void WorldRenderBridge::Render(const RuntimeOptions& options, eyelib::api::world::World& world,
                               const WorldAnimationBridge* animationBridge,
                               eyelib::api::AssetRegistry* assetRegistry,
                               uint32_t width, uint32_t height) {
    if (!m_ready) {
        return;
    }
    m_pipeline.SetMode(SceneModeToPipelineMode(world.GetRenderPipelineMode()));
    eyelib::api::RenderScene scene;
    BuildScene(world, options, animationBridge, assetRegistry, scene, nullptr);

    const bool useXR = m_xrProvider && m_xrProvider->IsSessionActive() && !m_frameSnapshot.xrCameras.empty();
    if (useXR && m_xrProvider->GetViewCount() >= 2u) {
        const uint32_t viewCount = m_xrProvider->GetViewCount();
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        if (!m_frameSnapshot.xrCameras.empty()) {
            nearPlane = m_frameSnapshot.xrCameras[0].component.nearPlane;
            farPlane = m_frameSnapshot.xrCameras[0].component.farPlane;
        }
        if (!m_xrProvider->BeginFrame()) {
            RenderSceneOnly(scene, width, height);
            return;
        }
        std::vector<eyelib::vr::XRTextureHandle> handles(static_cast<size_t>(viewCount), 0u);
        for (uint32_t i = 0; i < viewCount; ++i) {
            eyelib::vr::XRViewData viewData;
            if (!m_xrProvider->GetViewPoseFov(i, viewData)) continue;
            eyelib::vr::XRViewProjection xrCam = eyelib::vr::ViewProjectionFromXRView(viewData, nearPlane, farPlane);
            scene.camera.view = xrCam.view;
            scene.camera.projection = xrCam.projection;
            scene.camera.viewPos = xrCam.viewPos;
            scene.camera.nearPlane = xrCam.nearPlane;
            scene.camera.farPlane = xrCam.farPlane;

            uint32_t eyeW = 0, eyeH = 0;
            m_xrProvider->GetRecommendedViewSize(i, eyeW, eyeH);
            if (eyeW == 0) eyeW = 1;
            if (eyeH == 0) eyeH = 1;
            if (!m_xrEyeTargets[i] || m_xrEyeSize[i][0] != eyeW || m_xrEyeSize[i][1] != eyeH) {
                m_xrEyeTargets[i] = std::make_unique<eyelib::api::RenderTarget>();
                if (!m_xrEyeTargets[i]->Create(&m_pipeline, eyeW, eyeH,
                        std::vector<eyelib::api::TextureFormat>{eyelib::api::TextureFormat::RGBA16F},
                        eyelib::api::TextureFormat::Depth24, 0)) {
                    m_xrEyeTargets[i].reset();
                    continue;
                }
                m_xrEyeSize[i][0] = eyeW;
                m_xrEyeSize[i][1] = eyeH;
            }
            ApplyResize(eyeW, eyeH);
            if (!m_pipeline.HasValidOutput()) continue;
            m_pipeline.Render(scene);
            m_pipeline.BlitOutputTo(*m_xrEyeTargets[i]);
            void* native = m_xrEyeTargets[i]->GetColorTextureNativeHandle(0);
            handles[i] = native ? static_cast<eyelib::vr::XRTextureHandle>(reinterpret_cast<uintptr_t>(native)) : 0u;
        }
        m_xrProvider->EndFrame(viewCount, handles.data());
        ApplyResize(width, height);
        return;
    }

    // Stereo preview when XR camera present but no headset: single-pass stereo (one render, two viewports).
    if (!m_frameSnapshot.xrCameras.empty() && width >= 2 && height >= 1) {
        const auto& xrEntry = m_frameSnapshot.xrCameras[0];
        float nearPlane = xrEntry.component.nearPlane;
        float farPlane = xrEntry.component.farPlane;
        ::eyelib::world::components::Transform xrTransform;
        if (world.GetWorldTransform(xrEntry.entity, xrTransform)) {
            eyelib::vr::XRViewData leftData, rightData;
            eyelib::vr::StereoViewDataFromCenterPose(xrTransform.position, xrTransform.rotation,
                eyelib::vr::kDefaultStereoIPD, eyelib::vr::kDefaultStereoFovHalfX, eyelib::vr::kDefaultStereoFovHalfY,
                leftData, rightData);
            eyelib::vr::XRViewProjection leftProj = eyelib::vr::ViewProjectionFromXRView(leftData, nearPlane, farPlane);
            eyelib::vr::XRViewProjection rightProj = eyelib::vr::ViewProjectionFromXRView(rightData, nearPlane, farPlane);

            eyelib::api::CameraView leftCam, rightCam;
            leftCam.view = leftProj.view;
            leftCam.projection = leftProj.projection;
            leftCam.viewPos = leftProj.viewPos;
            leftCam.nearPlane = leftProj.nearPlane;
            leftCam.farPlane = leftProj.farPlane;
            rightCam.view = rightProj.view;
            rightCam.projection = rightProj.projection;
            rightCam.viewPos = rightProj.viewPos;
            rightCam.nearPlane = rightProj.nearPlane;
            rightCam.farPlane = rightProj.farPlane;

            ApplyResize(width, height);
            if (m_pipeline.HasValidOutput()) {
                m_pipeline.RenderStereo(scene, leftCam, rightCam);
                m_pipeline.BlitToScreen();
            }
            return;
        }
    }

    RenderSceneOnly(scene, width, height);
}

void WorldRenderBridge::ApplyResize(uint32_t width, uint32_t height) {
    if (!m_ready || width == 0 || height == 0) return;
    m_width = width;
    m_height = height;
    m_pipeline.Resize(width, height);
}

bool WorldRenderBridge::RenderSceneOnly(const eyelib::api::RenderScene& scene, uint32_t width, uint32_t height) {
    if (!m_ready) {
        return false;
    }
    if (width == 0 || height == 0) {
        return false;
    }
    if (width != m_width || height != m_height) {
        ApplyResize(width, height);
    }
    if (!m_pipeline.HasValidOutput()) {
        return false;
    }
    m_pipeline.Render(scene);
    m_pipeline.BlitToScreen();
    return true;
}

void WorldRenderBridge::Finish() {
    m_pipeline.Finish();
}

void* WorldRenderBridge::InsertFrameFence() {
    return m_pipeline.InsertFrameFence();
}

void WorldRenderBridge::WaitFence(void* fence) {
    m_pipeline.WaitFence(fence);
}

void WorldRenderBridge::WaitAndDestroyFence(void* fence) {
    m_pipeline.WaitAndDestroyFence(fence);
}

} // namespace openeye::player
