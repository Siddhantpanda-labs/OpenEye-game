# Material / deferred texture issue — repo comparison (Apr 2026)
## Compared
- **Main repo (OpenEye):** Vulkan + `GBufferPassSystem.cpp`, shaders under `EngineSource/assets/shaders/vulkan/passes/deferred/`.
- **OpenEye-game-forwardgl:** OpenGL + `GBufferPass.cpp`, shaders under `OpenEye-game-forwardgl/EngineSource/assets/shaders/opengl/passes/deferred/`.
Assumption: imported `.omesh` / materials are the same and fresh; differences are in **engine code paths**, not stale assets.
---
## What matches
- **Material map + per-submesh material IDs:** `InstantiateOmesh.cpp` builds `materialIdsByMeshIndex` the same way in both trees (walk mesh entries, submeshes, `materialIndex` → `.materialmap` → GUID or asset-relative path).
- **G-buffer vertex UV logic:** `gbuffer.vert` (OpenGL) vs Vulkan `gbuffer.vert` use the same `mix(UV0, UV1, …)`, scale/rotate/offset, and optional flip-V.
- **Per-draw material data:** Both passes read `u_UVTransform`, `u_TexCoordIndex`, etc. from `Material` / `MaterialInstance` and bind **albedo / normal / metallic-roughness** (plus flags) for the surface evaluation.
---
## What differs (important)
### 1. Deferred pass implementation (biggest structural difference)
| | ForwardGL | Main (Vulkan) |
|---|-----------|----------------|
| **File** | `eyelib/render/passes/GBufferPass.cpp` | `eyelib/render/passes/GBufferPassSystem.cpp` |
| **Textures (bindless)** | `shader->SetHandle64(...)` per texture slot each draw | **Not** set on Vulkan in `drawOne`; textures go through **descriptor set 1** |
| **Material UBO** | `cmd->BindUniformBuffer(materialBuffer, …)` per draw | UBO written via `UpdateForwardResourceSet` → `UpdateMaterialDescriptorSetWithDefaults` (`PassResourceBuilders.cpp`, binding 0) |
| **Model / UV** | Vertex uniforms (`u_Model`, …) | **Push constants** (`WriteGBufferGeometryPushConstants` in `PassPushConstants.h`): VS block **plus** fragment `u_HasAlbedoTexture` at offset **92** |
| **Extra** | — | Requires bindless heap + descriptor sets (frame / material / bindless / scene) before geometry runs |
ForwardGL always goes through the same uniform/handle path. Main **splits**: OpenGL-style uniforms + handles on non-Vulkan; **Vulkan-only** push constants + **sampled textures on set = 1** (`gbuffer.frag`).
### 2. Fragment shader shape
- **ForwardGL** `gbuffer.frag`: material-compiler placeholders (`@MATERIAL_DEFINES@`, many optional samplers); runtime uses **`uniform float u_HasAlbedoTexture`**.
- **Main Vulkan** `gbuffer.frag`: **fixed** `layout(set = 1, binding = 1..3)` samplers; **`layout(push_constant)`** for `u_HasAlbedoTexture` only (no uniform for that flag).
So a regression is more likely in **Vulkan descriptor updates**, **push constant / pipeline layout range**, or **runtime material → `GetTextureRHI`**, not in the Assimp-side `.materialmap` text itself (identical editor code).
### 3. Editor `InstantiateOmesh` — submesh index assignment
- **ForwardGL:** `AssignMeshIndices` maps **`meshPath` → global submesh start** only.
- **Main:** Same path **plus** if the prefab has **`meshIndex` ≥ 0**, it is remapped through **`BuildSubmeshStartByMeshEntryIndex`** (order of **mesh entries** inside `.omesh`).
So main can **diverge** from forwardGL when prefab `meshIndex` is present and does not match “path-only” resolution. That affects **which submesh + which row of `materialIdsByMeshIndex`** is used — classic “wrong atlas region / wrong material” for multi-part houses.
---
## Root cause found (Vulkan G-buffer push constants)

Separate SPIR-V modules are compiled **per stage**. If the fragment `layout(push_constant) uniform GBufferFragPush { float u_HasAlbedoTexture; }` has **no** `layout(offset = …)`, glslang places that float at **byte 0**. That **aliases** the vertex push at byte 0 (`u_Model` / instanced `u_UVTransform`). Result:

- **`u_HasAlbedoTexture`** was effectively read from **wrong bits** (e.g. first float of the model matrix or UV transform), so albedo sampling could be toggled wrong.
- **C++** wrote the real flag at offset **92** (`WriteGBufferGeometryPushConstants`), which the **fragment never read** if SPIR-V still said offset **0**.

**Instanced path:** `Shader::SetVec4` / `SetFloat` are **no-ops on Vulkan** (`VulkanDisablesLegacyUniforms`). `RenderInstanced` never called `WriteGBuffer*PushConstants`, so **material UV / atlas transform** never reached the GPU for batched instanced deferred draws.

**Skinned path:** `u_UsePerDrawTransformBuffer` was at offset **92**, same as the fragment float — fixed by moving the int to **offset 96** and writing **0** from C++ on normal draws (**1** for indirect per-draw transform).

### Fixes applied (code)

- `gbuffer.frag`, `gbuffer_skinned.frag`: `layout(offset = 92) float u_HasAlbedoTexture`.
- `gbuffer_instanced.frag`: `layout(offset = 28) float u_HasAlbedoTexture` (after 28-byte `GBufferInstancedPush`).
- `gbuffer_skinned.vert`: `layout(offset = 96) int u_UsePerDrawTransformBuffer`.
- `PassPushConstants.h`: `WriteGBufferInstancedGeometryPushConstants`, `kGBufferInstancedFragHasAlbedoOffset`, `kGBufferSkinnedUsePerDrawTransformOffset`.
- `GBufferPassSystem.cpp`: Vulkan **drawOne** writes **0** at offset **96**; **RenderInstanced** uses instanced push writer; **RenderIndirectWithPerDrawData** (skinned) sets push + **1** at **96**.

- **Pipeline:** `VulkanResourceSpaces.cpp` `CreateGBufferPipelineLayout` uses **one** push range, **256 bytes**, VS+FS — still sufficient.

---

## Submesh → material → atlas (per draw)

Intended flow for each **visible opaque** drawable (editor: `EditorSceneBuilder.cpp`):

1. **Renderable** supplies `meshAssetId`, **`materialAssetId`**, **`submeshIndex`** (from prefab / scene).
2. Cache resolves **one merged `Mesh`** + **one `Material`** for that entity (`RebuildDrawableStaticIfNeeded`: `LoadModelEntry`, then override material from `materialAssetId`).
3. **G-buffer `drawOne`:** `UpdateForwardResourceSet` → `UpdateMaterialDescriptorSetWithDefaults`: material **UBO** + **bindings 1–3** (`u_AlbedoTexture`, normal, MR) = that material’s **atlas (or concrete) textures**.
4. **Push:** `u_UVTransform`, `u_UVRotation`, `u_TexCoordIndex`, `u_FlipV` from the **same** material → vertex builds **`v_TexCoord`** in atlas space.
5. **`MeshHandle::Draw(device, submeshIndex)`** selects **index range** only; UVs come from vertex data × push; sampling uses **bound** textures.

So there is **no separate “submesh slot” in the shader** beyond geometry: correctness depends on **entity** having the **right** `(submeshIndex, materialAssetId)` pair. The pass itself **does** refresh descriptors + push **per drawable** before each draw.

---

## If issues remain after the fix

1. **Prefab / submesh:** Log `submeshIndex` + `materialAssetId` per entity; confirm `meshIndex` remapping in `InstantiateOmesh.cpp` matches import order.
2. **Shaders:** Ensure the runtime loads the updated GLSL (rebuild / clear cached SPIR-V if your workflow caches binaries).

---

## Short conclusion

- **Primary culprit (Vulkan deferred):** Fragment push had **no explicit `layout(offset = …)`**, so SPIR-V placed `u_HasAlbedoTexture` at **byte 0** and **aliased** vertex push data; C++ wrote the flag at **92**, which the FS did not use. That breaks albedo/texture toggling and atlas appearance.
- **Instanced G-buffer:** `SetVec4`/`SetFloat` are **no-ops on Vulkan**; **`RenderInstanced` did not write** UV push constants — fixed with `WriteGBufferInstancedGeometryPushConstants`.
- **Skinned:** `u_UsePerDrawTransformBuffer` overlapped byte **92**; moved to **offset 96** with C++ writes (0 normal, 1 indirect per-draw).
- **Importer / materialmap:** Still aligned with forwardGL; less likely the main issue once push layout is correct.