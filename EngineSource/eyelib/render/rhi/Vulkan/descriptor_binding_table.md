# Vulkan descriptor binding table (resource-space layouts)

Single source of truth for set and binding indices. Shaders and C++ must match.

**Rule:** Sets are organized by **lifetime** (frame / pass / material / textures / scene).

| Set | Role        | Lifetime   |
|-----|-------------|------------|
| 0   | Frame       | Once/frame |
| 1   | Pass        | Per pass   |
| 2   | Material    | Per draw   |
| 3   | Textures    | Bindless   |
| 4   | SceneBuffers| Frame/pass |

---

## Set 0 — Frame

| Binding | Type   | Content        |
|---------|--------|----------------|
| 0       | UBO    | Camera (view, projection, viewProj) |
| 1       | UBO    | Time / global lighting (optional)   |

---

## Set 1 — Pass (layout depends on pass type)

### ForwardPass

| Binding | Type   | Content        |
|---------|--------|----------------|
| 0       | Image  | Shadow map(s) / cascade array |
| 1       | SSBO   | Light list     |
| 2       | SSBO   | Cluster grid (optional) |
| 3       | Image  | IBL irradiance (cube) |
| 4       | Image  | IBL prefilter (cube) |
| 5       | Image  | BRDF LUT (2D) |

### PostPass

| Binding | Type   | Content        |
|---------|--------|----------------|
| 0       | Image  | Scene color    |
| 1       | Image  | Scene depth    |
| 2       | Image  | Velocity (optional) |

### ShadowPass

| Binding | Type   | Content        |
|---------|--------|----------------|
| 0       | UBO    | Cascade/shadow params (optional) |

### LightCullPass (compute)

| Binding | Type   | Content        |
|---------|--------|----------------|
| 0       | UBO    | ParamsBlock (view, tile count, etc.) |
| 1       | SSBO   | Light list output |

### DeferredLightingPass

| Binding | Type   | Content        |
|---------|--------|----------------|
| 0       | Image  | GBuffer position |
| 1       | Image  | GBuffer normal   |
| 2       | Image  | GBuffer albedo   |
| 3       | Image  | GBuffer PBR      |
| 4       | Image  | Shadow map (cascade array) |
| 5       | SSBO   | Scene lighting   |
| 6       | Buffer | Light space (cascade matrices) |
| 7       | Image  | IBL irradiance   |
| 8       | Image  | IBL prefilter    |
| 9       | Image  | BRDF LUT         |

---

## Set 2 — Material

| Binding | Type   | Content        |
|---------|--------|----------------|
| 0       | UBO    | Material constants |
| 1       | Image  | Albedo / base color |
| 2       | Image  | Normal (optional) |
| 3       | Image  | Roughness/Metal (optional) |

---

## Set 3 — Textures (bindless)

Existing bindless heap: textures binding 0, buffers binding 1. No change.

---

## Set 4 — SceneBuffers

| Binding | Type   | Content        |
|---------|--------|----------------|
| 0       | SSBO   | Draw list (GPU cull input) |
| 1       | SSBO   | Instance / transform buffer; or bone matrices (skinned Forward/ForwardPlus/GBuffer) |
| 2       | SSBO   | Light buffer   |
| 3       | SSBO   | Cluster data (optional) |

---

## Pipeline layout kinds (which sets each uses)

| Kind              | Sets used        |
|-------------------|------------------|
| Legacy            | (legacy set0 + bindless) |
| Forward           | 0, 1, 2, 3, 4   |
| Post              | 0, 1            |
| Shadow            | 0, 1, 4         |
| GBuffer           | 0, 2, 3, 4      |
| DeferredLighting  | 0, 1            |
| ComputeLightCull  | 0, 1, 4         |
| ComputeGpuCull    | 0, 4            |

---

## Push constants

- **Size:** ≤ 128 bytes for all new pipelines.
- **Use for:** draw ID, material index, pass flags.
- **Everything else:** use buffers in Frame / Pass / Material / SceneBuffers.
