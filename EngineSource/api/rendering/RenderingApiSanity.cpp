// =====================================================================================
// RenderingApiSanity.cpp — M1.9 compile sanity TU.
//
// Purpose:
//   Prove that every public header in `api/rendering/` is independently includable, all
//   public PIMPL handle types can be default-constructed without an initialised library,
//   all observation methods on a default (invalid) handle are safe, and the friend-only
//   `internal/ResourceAccess.h` seam compiles when used from `api/rendering/*.cpp`.
//
//   This TU never runs at runtime — it only needs to compile + link cleanly. The single
//   function below is `[[maybe_unused]]` so the linker may discard it. The presence of
//   `static_assert`s and the act of compiling the body is the actual test.
//
// Why here (and not in a separate test target):
//   The PIMPL boundary is the contract we care about. A separate test that links
//   EyelibRenderingAPI proves linkage; this in-library TU additionally proves header
//   compose + impl-side access seam. M3 / M4 will add consumer-side integration tests
//   when the editor + player land their first migrated translation units.
// =====================================================================================

#include "api/rendering/CameraView.h"
#include "api/rendering/Light.h"
#include "api/rendering/Material.h"
#include "api/rendering/MaterialInstance.h"
#include "api/rendering/MaterialRecord.h"
#include "api/rendering/Mesh.h"
#include "api/rendering/MeshData.h"
#include "api/rendering/RenderBackend.h"
#include "api/rendering/RenderDevice.h"
#include "api/rendering/RenderPipeline.h"
#include "api/rendering/RenderScene.h"
#include "api/rendering/RenderTarget.h"
#include "api/rendering/Renderer.h"
#include "api/rendering/ResourceCache.h"
#include "api/rendering/Shader.h"
#include "api/rendering/Texture2D.h"
#include "api/rendering/Texture3D.h"
#include "api/rendering/TextureArray.h"
#include "api/rendering/TextureCube.h"
#include "api/rendering/TextureData.h"
#include "api/rendering/internal/ResourceAccess.h"

#include <type_traits>

namespace eyelib {
namespace api {
namespace rendering {
namespace sanity_detail {

// Compile-time invariants on the public surface ----------------------------------------

// Opaque handles must remain a single-pointer ABI shape (already locked in RenderingApi.cpp).
static_assert(sizeof(RenderDevice) == sizeof(void*),
              "RenderDevice must remain a single-pointer opaque handle.");

// Handle types are default-constructible (invalid until populated by the M3 factory).
static_assert(std::is_default_constructible<Mesh>::value,             "Mesh must be default-constructible.");
static_assert(std::is_default_constructible<Material>::value,         "Material must be default-constructible.");
static_assert(std::is_default_constructible<MaterialInstance>::value, "MaterialInstance must be default-constructible.");
static_assert(std::is_default_constructible<Texture2D>::value,        "Texture2D must be default-constructible.");
static_assert(std::is_default_constructible<TextureCube>::value,      "TextureCube must be default-constructible.");
static_assert(std::is_default_constructible<Texture3D>::value,        "Texture3D must be default-constructible.");
static_assert(std::is_default_constructible<TextureArray>::value,     "TextureArray must be default-constructible.");
static_assert(std::is_default_constructible<Shader>::value,           "Shader must be default-constructible.");
static_assert(std::is_default_constructible<Renderer>::value,         "Renderer must be default-constructible.");
static_assert(std::is_default_constructible<RenderPipeline>::value,   "RenderPipeline must be default-constructible.");
static_assert(std::is_default_constructible<RenderScene>::value,      "RenderScene POD must be default-constructible.");
static_assert(std::is_default_constructible<DrawableInstance>::value, "DrawableInstance POD must be default-constructible.");
static_assert(std::is_default_constructible<CameraView>::value,       "CameraView POD must be default-constructible.");

// Resource handles are copyable + movable.
static_assert(std::is_copy_constructible<Mesh>::value, "Mesh must be copy-constructible.");
static_assert(std::is_move_constructible<Mesh>::value, "Mesh must be move-constructible.");
static_assert(std::is_copy_assignable<Mesh>::value,    "Mesh must be copy-assignable.");
static_assert(std::is_move_assignable<Mesh>::value,    "Mesh must be move-assignable.");

// Renderer + RenderPipeline are non-copyable but movable (PIMPL via unique_ptr<Impl>).
static_assert(!std::is_copy_constructible<Renderer>::value,       "Renderer must be non-copyable.");
static_assert(!std::is_copy_constructible<RenderPipeline>::value, "RenderPipeline must be non-copyable.");
static_assert(std::is_move_constructible<RenderPipeline>::value,  "RenderPipeline must be move-constructible.");
static_assert(std::is_move_assignable<RenderPipeline>::value,     "RenderPipeline must be move-assignable.");

// Default Mode is Hybrid (parity with legacy default; documented in RenderPipeline.h).
static_assert(static_cast<int>(RenderPipeline::Mode::Hybrid) == 0,
              "RenderPipeline::Mode::Hybrid must be the first / default enumerator.");

// DrawLayer enum order matches the 1:1 mapping documented in RenderScene.h
// (Opaque, AlphaTest, Transparent, Sky).
static_assert(static_cast<int>(DrawLayer::Opaque)      == 0, "DrawLayer::Opaque == 0");
static_assert(static_cast<int>(DrawLayer::AlphaTest)   == 1, "DrawLayer::AlphaTest == 1");
static_assert(static_cast<int>(DrawLayer::Transparent) == 2, "DrawLayer::Transparent == 2");
static_assert(static_cast<int>(DrawLayer::Sky)         == 3, "DrawLayer::Sky == 3");

// draw_instance_flags bit layout matches the internal bit layout documented in RenderScene.h.
static_assert(draw_instance_flags::kNone        == 0u, "draw_instance_flags::kNone == 0");
static_assert(draw_instance_flags::kDoubleSided == 1u, "draw_instance_flags::kDoubleSided == 1");
static_assert(draw_instance_flags::kCastShadow  == 2u, "draw_instance_flags::kCastShadow == 2");

// MaterialAlphaMode numeric values must mirror eyelib::rendering::materials::MaterialAlphaMode 1:1
// (RenderPipeline::SetMaterialRecord casts directly without a switch).
static_assert(static_cast<uint32_t>(MaterialAlphaMode::Opaque) == 0u, "MaterialAlphaMode::Opaque == 0");
static_assert(static_cast<uint32_t>(MaterialAlphaMode::Mask)   == 1u, "MaterialAlphaMode::Mask == 1");
static_assert(static_cast<uint32_t>(MaterialAlphaMode::Blend)  == 2u, "MaterialAlphaMode::Blend == 2");

// New POD descriptor types are default-constructible (used by editor / player as plain data).
static_assert(std::is_default_constructible<MeshData>::value,       "MeshData must be default-constructible.");
static_assert(std::is_default_constructible<TextureData>::value,    "TextureData must be default-constructible.");
static_assert(std::is_default_constructible<MaterialRecord>::value, "MaterialRecord must be default-constructible.");

// M4.0a light PODs are default-constructible and trivially copyable (vectors of these
// flow through `RenderScene` per frame; the adapter does a structural copy into the new
// module's `SceneLighting` state).
static_assert(std::is_default_constructible<DirectionalLight>::value, "DirectionalLight must be default-constructible.");
static_assert(std::is_default_constructible<PointLight>::value,       "PointLight must be default-constructible.");
static_assert(std::is_default_constructible<SpotLight>::value,        "SpotLight must be default-constructible.");
static_assert(std::is_trivially_copyable<DirectionalLight>::value,    "DirectionalLight must be trivially copyable.");
static_assert(std::is_trivially_copyable<PointLight>::value,          "PointLight must be trivially copyable.");
static_assert(std::is_trivially_copyable<SpotLight>::value,           "SpotLight must be trivially copyable.");

// ResourceCache is non-copyable / non-movable (owns pipeline-attachment list with stable
// `this` pointer that the cache hands out via `IMeshHandleResolver*`).
static_assert(!std::is_copy_constructible<ResourceCache>::value, "ResourceCache must be non-copyable.");
static_assert(!std::is_move_constructible<ResourceCache>::value, "ResourceCache must be non-movable.");

// Runtime sanity exercise --------------------------------------------------------------
//
// Default-construct every public type, query its observation methods, and prove the
// friend-only Wrap seam compiles. Never called; presence of the body is the test.

[[maybe_unused]] static void CompileSanity() {
    // Opaque value types.
    RenderDevice  device{};      (void)static_cast<bool>(device);
    RenderBackend backend = RenderBackend::Default; (void)backend;
    CameraView    camera{};      (void)camera.nearPlane;

    // Resource handles default to invalid.
    Mesh             mesh{};             if (mesh.IsValid())             return;  // unreachable
    Material         material{};         if (material.IsValid())         return;
    MaterialInstance instance{};         if (instance.IsValid())         return;
    Texture2D        tex2{};             if (tex2.IsValid())             return;
    TextureCube      texCube{};          if (texCube.IsValid())          return;
    Texture3D        tex3{};             if (tex3.IsValid())             return;
    TextureArray     texArr{};           if (texArr.IsValid())           return;
    Shader           shader{};           if (shader.IsValid())           return;

    (void)mesh.GetMeshHandle();
    (void)material.GetMaterialId();
    (void)instance.GetMaterialId();
    (void)tex2.GetWidth(); (void)tex2.GetNativeHandle();
    (void)texCube.GetSize();
    (void)tex3.GetDepth();
    (void)texArr.GetLayerCount();

    // Material setters on an invalid handle are safe no-ops (proven by IsValid guard
    // in Material.cpp).
    material.SetAlbedo({1.0f, 1.0f, 1.0f});
    instance.SetAlphaOverride(0.5f);
    material.SetAlbedoTexture(tex2);  // invalid texture → cleared slot

    // Renderer + RenderPipeline construct without a device. Public methods are safe
    // pre-Initialize.
    Renderer       renderer{};
    RenderPipeline pipeline{};
    if (renderer.Initialize()) renderer.Shutdown();
    pipeline.Shutdown();
    pipeline.SetMode(RenderPipeline::Mode::Hybrid);
    (void)pipeline.GetMode();
    pipeline.SetSkyboxBrightness(1.0f);
    (void)pipeline.GetSkyboxBrightness();
    pipeline.UpdateUploads();
    pipeline.BlitToScreen();
    pipeline.RestoreDefaultFramebufferAndViewport(0, 0);
    pipeline.Finish();
    (void)pipeline.HasValidOutput();
    (void)pipeline.OwnsDevice();
    (void)pipeline.GetDevice();
    (void)pipeline.GetBackendName();
    (void)pipeline.GetBackend();
    (void)pipeline.GetOutputColorTextureNativeHandle();
    (void)pipeline.GetShadowDepthMapNativeViewForDebug();
    (void)pipeline.GetShadowCascadeCountForDebug();
    (void)pipeline.GetShadowMapResolutionForDebug();
    (void)pipeline.GetFrameGraphPasses();
    pipeline.ReloadShaders();

    RenderScene scene{};
    scene.opaque.push_back(DrawableInstance{});
    // M4.0a — exercise the lights surface so the structural-copy + push-to-SceneLighting
    // path stays compile-clean. Render() on a default pipeline is a no-op (no world),
    // so the lighting push is reached only when initialised — both branches still compile.
    scene.directionalLightActive = true;
    scene.directionalLight       = DirectionalLight{};
    scene.pointLights.push_back(PointLight{});
    scene.spotLights.push_back(SpotLight{});
    pipeline.Render(scene);
    pipeline.Render(scene, nullptr, nullptr);

    uint32_t pickId = 0;
    (void)pipeline.ReadPixelEntityId(0, 0, &pickId);
    pipeline.RenderOutlineFromIdBuffer(0u);

    // Friend-only construction seam: Wrap with null shared_ptrs returns invalid handles.
    {
        Mesh             m  = MeshAccess::Wrap(nullptr, 0u);             (void)m.IsValid();
        Material         mt = MaterialAccess::Wrap(nullptr, eyelib::api::AssetId::Invalid(), 0u); (void)mt.IsValid();
        MaterialInstance mi = MaterialInstanceAccess::Wrap(nullptr, 0u); (void)mi.IsValid();
        Texture2D        t2 = Texture2DAccess::Wrap(nullptr, 0u);        (void)t2.IsValid();
        (void)Texture2DAccess::Unwrap(t2);
        (void)Texture2DAccess::GetCacheToken(t2);
        TextureCube      tc = TextureCubeAccess::Wrap(nullptr);          (void)tc.IsValid();
        Texture3D        t3 = Texture3DAccess::Wrap(nullptr);            (void)t3.IsValid();
        TextureArray     ta = TextureArrayAccess::Wrap(nullptr);         (void)ta.IsValid();
        Shader           sh = ShaderAccess::Wrap(nullptr);               (void)sh.IsValid();
    }

    // ResourceCache: cold-path construction (no device) yields IsValid() == false; all
    // Create* return invalid handles after a one-shot warning. Lookup methods on an empty
    // cache also return invalid handles. Material handles vended by the cache route their
    // setters through ResourceCacheAccess::MutateMaterial, which is a no-op on unknown ids.
    {
        ResourceCache cache{RenderDevice{}};  // null device → cold-path mode
        if (cache.IsValid()) return;          // unreachable

        const eyelib::api::AssetId id{};
        MeshData      meshData{};
        TextureData   textureData{};
        MaterialRecord matRec{};

        Mesh      m  = cache.CreateMesh(id, meshData);     (void)m.IsValid();
        Texture2D t  = cache.CreateTexture(id, textureData); (void)t.IsValid();
        Material  mt = cache.CreateMaterial(id, matRec);   (void)mt.IsValid();
        (void)cache.GetMesh(id);
        (void)cache.GetTexture(id);
        (void)cache.GetMaterial(id);
        (void)cache.HasMesh(id);
        (void)cache.HasTexture(id);
        (void)cache.HasMaterial(id);
        cache.Remove(id);
        cache.OnDeviceLost();

        // Pipeline attach / detach is a public surface; safe pre-Initialize on a default
        // pipeline (no-ops because the pipeline has no world / features yet).
        RenderPipeline pl{};
        pl.AttachResourceCache(cache);
        pl.DetachResourceCache(cache);
        pl.SetMaterialRecord(0u, matRec);
    }
}

} // namespace sanity_detail
} // namespace rendering
} // namespace api
} // namespace eyelib
