#pragma once

// =====================================================================================
// internal/ResourceAccess.h
// =====================================================================================
// Friend-only seam for constructing public `api/rendering` PIMPL handles
// (`Mesh`, `Material`, `MaterialInstance`, `Texture2D`, `TextureCube`, `Texture3D`,
// `TextureArray`, `Shader`) from internal `eyelib::rendering` resources.
//
// **This header is private to the `EyelibRenderingAPI` library.** Including it from
// outside `api/rendering/*.cpp` is a build-discipline violation; the editor and player
// must obtain handles through factory entry points exposed in higher-level public
// headers (those factories land when the editor GPU resource cache migrates). Keeping
// construction friend-only avoids committing to a factory shape before consumers exist.
//
// Each `*Access` type is declared a `friend` of its corresponding public handle in the
// matching public header. The `*Access` types are defined here as inline static helpers
// that wrap an internal `std::shared_ptr<eyelib::rendering::TYPE>` (and an opaque uint32
// token where applicable) into the handle's `Impl`. Callers in `api/rendering/*.cpp`
// include this header along with the relevant internal header.
// =====================================================================================

#include "api/asset/AssetId.h"
#include "api/rendering/Material.h"
#include "api/rendering/MaterialInstance.h"
#include "api/rendering/MaterialRecord.h"
#include "api/rendering/Mesh.h"
#include "api/rendering/Shader.h"
#include "api/rendering/Texture2D.h"
#include "api/rendering/Texture3D.h"
#include "api/rendering/TextureArray.h"
#include "api/rendering/TextureCube.h"

#include <cstdint>
#include <functional>
#include <memory>

// Internal type forward decls (no internal headers leaked into the access header itself;
// the .cpp call sites include the matching internal headers and pass typed shared_ptrs).
namespace eyelib { namespace rendering {
class Mesh;
class Material;
class MaterialInstance;
class Texture2D;
class Texture3D;
class TextureArray;
class TextureCube;
class Shader;

class IMeshHandleResolver;  // M3.7b — implemented privately by `ResourceCache::Impl`
}}

namespace eyelib {
namespace api {
namespace rendering {

class RenderPipeline;  // M3.7b — pipeline registration target
class ResourceCache;   // M3.7b — friend-only access seam target

// Each *Access provides a single static `Wrap` that constructs a public handle from
// internal shared_ptr ownership + (where applicable) a uint32 handle/material token.
// Definitions live alongside the matching public handle's .cpp impl (same TU as the
// `Impl` struct).

class MeshAccess {
public:
    static Mesh Wrap(std::shared_ptr<eyelib::rendering::Mesh> internalMesh, uint32_t meshHandleToken);
};

class MaterialAccess {
public:
    /** Construct a Material handle bound to a `ResourceCache` entry. The cache owns the
     *  underlying `MaterialRecord`; the handle's setters mutate that record and ask the
     *  cache to re-publish it to every attached pipeline. `cache` may be null for the
     *  invalid-default handle path; that handle reports `IsValid() == false` and all
     *  setters become no-ops. */
    static Material Wrap(ResourceCache* cache,
                         const eyelib::api::AssetId& assetId,
                         uint32_t materialIdToken);
};

class MaterialInstanceAccess {
public:
    static MaterialInstance Wrap(std::shared_ptr<eyelib::rendering::MaterialInstance> internalInstance,
                                 uint32_t materialIdToken);
};

class Texture2DAccess {
public:
    /** Wrap an internal texture and tag it with a cache token (default 0 = no cache).
     *  The token is what `MaterialRecord::*Texture` slots store; resolution back to a
     *  GPU texture is the new module's responsibility (MaterialTable + binding feature). */
    static Texture2D Wrap(std::shared_ptr<eyelib::rendering::Texture2D> internalTexture,
                          uint32_t cacheToken = 0u);
    /** Extract the internal shared_ptr from a public handle (null when invalid). Used
     *  by material setters to thread textures into the internal Material API without
     *  exposing a separate friend per call site. */
    static std::shared_ptr<eyelib::rendering::Texture2D> Unwrap(const Texture2D& publicTexture);
    /** Cache token tagged at `Wrap` time. 0 when the texture was not vended by a cache. */
    static uint32_t GetCacheToken(const Texture2D& publicTexture) noexcept;
};

class TextureCubeAccess {
public:
    static TextureCube Wrap(std::shared_ptr<eyelib::rendering::TextureCube> internalTexture);
};

class Texture3DAccess {
public:
    static Texture3D Wrap(std::shared_ptr<eyelib::rendering::Texture3D> internalTexture);
};

class TextureArrayAccess {
public:
    static TextureArray Wrap(std::shared_ptr<eyelib::rendering::TextureArray> internalTexture);
};

class ShaderAccess {
public:
    static Shader Wrap(std::shared_ptr<eyelib::rendering::Shader> internalShader);
};

/**
 * @brief Friend-only access seam for `ResourceCache`.
 *
 * Two consumer groups inside `EyelibRenderingAPI`:
 *   - `RenderPipeline.cpp` calls `GetMeshResolver`, `RegisterPipeline`, and
 *     `UnregisterPipeline` from `AttachResourceCache` / `DetachResourceCache` /
 *     `Shutdown`. The cache owns the resolver instance and the attached-pipeline list.
 *   - `Material.cpp` calls `MutateMaterial` whenever a public `Material` setter fires.
 *     The lambda mutates the cache-owned `MaterialRecord` in place; the cache then
 *     re-publishes the updated row to every attached pipeline.
 *
 * Outside `EyelibRenderingAPI` this header is private (matches the invariant documented
 * at the top of this file).
 */
class ResourceCacheAccess {
public:
    /** Pipeline registration. `RegisterPipeline` also primes the pipeline's MaterialTable
     *  with all currently-known records; `UnregisterPipeline` is silent on unknown pointers. */
    static void RegisterPipeline(ResourceCache& cache, RenderPipeline* pipeline);
    static void UnregisterPipeline(ResourceCache& cache, RenderPipeline* pipeline);

    /** Mesh resolver used by raster + shadow features. Lifetime is the cache's lifetime;
     *  the caller stores a borrowed pointer per the `IMeshHandleResolver` contract. */
    static eyelib::rendering::IMeshHandleResolver* GetMeshResolver(ResourceCache& cache);

    /** Mutate the `MaterialRecord` stored under `assetId` and re-publish it to every
     *  attached pipeline. No-op when `assetId` is unknown. */
    static void MutateMaterial(ResourceCache& cache,
                               const eyelib::api::AssetId& assetId,
                               const std::function<void(MaterialRecord&)>& mutator);
};

} // namespace rendering
} // namespace api
} // namespace eyelib
