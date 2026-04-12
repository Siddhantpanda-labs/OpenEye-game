# Vulkan descriptor binding table (resource-space layouts)

This document is the authoritative ABI contract for Vulkan descriptor sets and push constants.
Shaders, C++ descriptor updates, and pipeline layout creation must match this file exactly.

## Contract metadata

- ABI name: `OpenEye.ResourceSpaces`
- ABI version: `v1`
- Backward compatibility: none unless version is explicitly bumped
- Ownership: render/rhi + renderer teams

---

## Global rules (non-negotiable)

1. Sets are organized by lifetime, not by feature.
2. Every declared binding index must always be valid at bind time.
3. Optional resources still require a valid descriptor write (dummy fallback texture/buffer).
4. Shader reflection, C++ descriptor writes, and layout factories must agree 1:1.
5. New code must not introduce `Legacy` descriptor paths.

**Pipeline-specific set numbering:** The table below is the **Forward** five-set layout (`CreateForwardPipelineLayout`). **GBuffer** uses four sets: 0 Frame, 1 Material, **2 bindless**, 3 SceneBuffers (`CreateGBufferPipelineLayout`). `ICommandList::BindBindlessHeap` (Vulkan) binds the heap at set **3** for Forward and set **2** for GBuffer after the matching pipeline is bound; prefer `BindDescriptorSets(0, N, …)` with all sets.

| Set | Role         | Lifetime   |
| --- | ------------ | ---------- |
| 0   | Frame        | Once/frame |
| 1   | Pass         | Per pass   |
| 2   | Material     | Per draw   |
| 3   | Textures     | Bindless   |
| 4   | SceneBuffers | Frame/pass |

---

## Set 0 - Frame

| Binding | Type | Required | Content                             |
| ------- | ---- | -------- | ----------------------------------- |
| 0       | UBO  | Yes      | Camera (view, projection, viewProj) |
| 1       | UBO  | Yes      | Global frame data (time/exposure/etc.) |

---

## Set 1 - Pass (layout per pass kind)

### ForwardPass

| Binding | Type  | Required | Content                            |
| ------- | ----- | -------- | ---------------------------------- |
| 0       | Image | Yes      | Shadow map(s) / cascade array      |
| 1       | SSBO  | Yes      | Light list / scene lighting payload |
| 2       | SSBO  | Yes      | Cluster grid or dummy SSBO         |
| 3       | Image | Yes      | IBL irradiance cube or dummy       |
| 4       | Image | Yes      | IBL prefilter cube or dummy        |
| 5       | Image | Yes      | BRDF LUT or dummy                  |

### PostPass

| Binding | Type  | Required | Content                  |
| ------- | ----- | -------- | ------------------------ |
| 0       | Image | Yes      | Scene color              |
| 1       | Image | Yes      | Scene depth              |
| 2       | Image | Yes      | Velocity (or dummy)      |
| 3       | Image | Yes      | Albedo (SSR path) or dummy |
| 4       | Image | Yes      | PBR (SSR path) or dummy  |
| 5       | Image | Yes      | Scene copy or dummy      |
| 6       | Image | Yes      | Skybox cube/dummy        |

### ShadowPass

| Binding | Type | Required | Content                                  |
| ------- | ---- | -------- | ---------------------------------------- |
| 0       | UBO  | Yes      | Cascade/shadow params (or dummy/default) |

### LightCullPass (compute)

| Binding | Type | Required | Content                              |
| ------- | ---- | -------- | ------------------------------------ |
| 0       | UBO  | Yes      | ParamsBlock (view, tile count, etc.) |
| 1       | SSBO | Yes      | Light list output                    |

### DeferredLightingPass

| Binding | Type   | Required | Content                        |
| ------- | ------ | -------- | ------------------------------ |
| 0       | Image  | Yes      | GBuffer position               |
| 1       | Image  | Yes      | GBuffer normal                 |
| 2       | Image  | Yes      | GBuffer albedo                 |
| 3       | Image  | Yes      | GBuffer PBR                    |
| 4       | Image  | Yes      | Shadow map (cascade array)     |
| 5       | SSBO   | Yes      | Scene lighting                 |
| 6       | Buffer | Yes      | Light space matrices           |
| 7       | Image  | Yes      | IBL irradiance                 |
| 8       | Image  | Yes      | IBL prefilter                  |
| 9       | Image  | Yes      | BRDF LUT                       |

---

## Set 2 - Material

| Binding | Type  | Required | Content                                   |
| ------- | ----- | -------- | ----------------------------------------- |
| 0       | UBO   | Yes      | Material constants                        |
| 1       | Image | Yes      | Albedo / base color (or fallback)         |
| 2       | Image | Yes      | Normal (or fallback flat normal)          |
| 3       | Image | Yes      | Roughness/Metal (or fallback packed map)  |

---

## Set 3 - Textures (bindless)

Bindless heap remains stable:

- Binding 0: sampled textures table
- Binding 1: storage/buffer table

All pipelines that include set 3 must bind a valid set (real or empty bindless set).

---

## Set 4 - SceneBuffers

| Binding | Type | Required | Content                                                           |
| ------- | ---- | -------- | ----------------------------------------------------------------- |
| 0       | SSBO | Yes      | Draw list / indirect metadata                                     |
| 1       | SSBO | Yes      | Instance transforms or bones payload                              |
| 2       | SSBO | Yes      | Scene light buffer                                                |
| 3       | SSBO | Yes      | Cluster/auxiliary scene data (or dummy)                          |

---

## Standalone layouts

### IBLPass

| Binding | Type  | Required | Content                      |
| ------- | ----- | -------- | ---------------------------- |
| 0       | Image | Yes      | Environment cube             |

Push constants:

- `u_Projection`: offset 0, 64 bytes
- `u_View`: offset 64, 64 bytes
- Max used: 128 bytes

### OutlinePost

| Binding | Type  | Required | Content         |
| ------- | ----- | -------- | --------------- |
| 0       | Image | Yes      | Scene color     |
| 1       | Image | Yes      | Entity ID R32UI |

Push constants:

- `u_SelectedID`, `u_OutlineColor`, `u_InvSize`
- Total must remain <= 256 bytes

---

## Pipeline layout kinds (required set topology)

| Kind              | Sets used |
| ----------------- | --------- |
| PushOnly          | none      |
| IBL               | 0         |
| OutlinePost       | 0         |
| Forward           | 0,1,2,3,4 |
| Post              | 0,1       |
| Shadow            | 0,1,4     |
| GBuffer           | 0,2,3,4   |
| DeferredLighting  | 0,1       |
| ComputeLightCull  | 0,1,4     |
| ComputeGpuCull    | 0,4       |

`Legacy` is treated as deprecated alias of `PushOnly` and is migration-only.

---

## Push constant policy

- Hard limit: <= 256 bytes per pipeline layout.
- Use only for tiny high-frequency values (draw ID, flags, tiny per-draw payload).
- Do not use push constants as primary transport for frame/pass/material data.
- Preferred data hierarchy:
  1. Frame/Pass/Material/SceneBuffers descriptors
  2. Push constants only for minimal hot fields

---

## Validation and CI gates

1. Shader reflection check: every `(set,binding,type)` in shaders matches this table.
2. Descriptor write check: every required binding is updated each frame/pass.
3. Pipeline layout check: kind -> set count must match this table.
4. Runtime bind check: no draw/dispatch with missing required sets.
5. Fallback check: optional features must bind explicit dummy descriptors.

If any gate fails, the ABI is considered broken.

---

## Migration policy (legacy removal)

- New code: must use resource-space layouts only.
- No new `Legacy` pipeline usage allowed.
- Existing `Legacy` paths are temporary and must be migrated pass-by-pass.
- Once all active passes are migrated, remove `Legacy` alias and related code paths.

---

## Change management

Any modification to set/binding/type semantics requires:

1. ABI version bump (`v1 -> v2`)
2. Synchronized updates to:
   - VulkanResourceSpaces layout factories
   - shader declarations
   - descriptor updates in renderer/passes
   - validation tests

Without these, the change is invalid.

