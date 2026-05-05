Your **Forward+ pipeline is already architecturally solid**. What you built is close to what many modern engines use. To push it toward **AAA-level**, the improvements fall into **five areas**: lighting scalability, GPU efficiency, shading quality, memory/bandwidth, and pipeline organization.

I’ll keep it **practical and ordered**.

---

# 1. Upgrade the light culling (core improvement)

Your clustered grid:

```
16×16 tiles
24 depth slices
```

This is good, but AAA engines optimize it further.

### What to change

**A. GPU-only light culling**

Right now you have:

```
GPU path OR CPU fallback
```

For high-end rendering you should prefer:

```
GPU compute light culling always
```

CPU culling becomes a debug fallback.

Reason:

```
CPU → memory upload every frame
GPU → zero CPU cost
```

---

### B. Compact cluster storage

Your current layout:

```
255 indices per cluster
```

Better structure:

```
cluster offset
cluster count
```

Example:

```
ClusterHeader
{
    uint offset;
    uint count;
}
```

And a global light list:

```
lightIndices[]
```

This avoids wasting space.

Current worst case:

```
clusterCount × 255
```

Better:

```
clusterCount × averageLights
```

This reduces memory and bandwidth.

---

# 2. Use wave/warp friendly compute kernels

Your compute culling likely loops like:

```
for light in lights
```

Better algorithm:

```
parallel light assignment
```

Pattern used in modern engines:

```
each thread = cluster
test lights
append to cluster list
```

Or:

```
each thread = light
determine clusters
append
```

Pick whichever produces **less divergence**.

---

# 3. Improve cluster depth distribution

Your current:

```
24 slices
log distribution
```

Good.

AAA engines usually use:

```
32 slices
```

This improves:

```
spot lights
small near lights
```

without large cost.

Suggested config:

```
tileSize = 16
depthSlices = 32
```

---

# 4. Improve batching system

Your batching key:

```
(mesh, material, submesh)
```

Better batching:

```
(mesh, materialShaderVariant)
```

Then push per-instance data via **instance buffers**.

This reduces:

```
shader binds
pipeline changes
```

---

# 5. Move material parameters to UBO

Right now your fragment shader has:

```
MaterialBlock (std140)
```

But make sure:

```
one UBO per material
```

Bound once per batch.

Avoid:

```
many uniform calls
```

---

# 6. Use bindless textures (optional but powerful)

Instead of:

```
sampler2D bound per draw
```

Use:

```
bindless texture indices
```

Material stores:

```
albedoTextureIndex
normalTextureIndex
```

This removes many state changes.

---

# 7. Improve reflection system

Your reflection stack should become:

```
SSR
↓
Reflection Probe
↓
IBL Sky
```

But also add:

```
parallax corrected probes
```

Without parallax correction probes look wrong indoors.

---

# 8. Add light types AAA engines use

Right now you have:

```
directional
point
spot
```

Add:

```
area lights
tube lights
rect lights
```

These are used heavily in modern scenes.

---

# 9. Add light range culling

Before cluster assignment do quick rejection:

```
distance test
```

Lights outside camera frustum should never reach cluster pass.

This saves compute time.

---

# 10. Implement shadow improvements

Shadow quality matters more than light count.

Add:

```
PCF filtering
PCSS soft shadows
contact shadows
```

Later:

```
VSM / EVSM shadows
```

---

# 11. Improve IBL integration

Ensure specular IBL uses:

```
prefiltered cubemap
BRDF LUT
roughness mip selection
```

Which it looks like you already do.

---

# 12. Add GPU light list limits

Avoid pathological cases.

Clamp:

```
max lights per cluster = 64
```

Beyond that fallback to:

```
global light
```

This prevents GPU stalls.

---

# 13. Use depth prepass for large scenes

Forward+ sometimes benefits from:

```
depth prepass
```

Advantages:

```
early z rejection
better overdraw handling
```

Useful for dense scenes.

---

# 14. Add volumetric lighting

AAA lighting always includes:

```
volumetric fog
light shafts
god rays
```

These integrate well with clustered lighting.

---

# 15. Add temporal AA

Forward pipelines especially need:

```
TAA
```

Without it:

```
specular aliasing
shimmering
```

---

# 16. Introduce light importance sorting

When cluster overflow happens prioritize:

```
closest lights
brightest lights
shadow casting lights
```

This keeps lighting stable.

---

# 17. Use half resolution lighting (optional)

For extremely heavy scenes:

```
compute lighting at half resolution
```

Then upscale.

Useful for mobile or VR.

---

# 18. Optimize memory layout

Align buffers for GPU cache:

```
PointLight struct = 32 bytes
SpotLight struct = 48 or 64 bytes
```

Avoid odd sizes.

---

# 19. Future upgrade: tiled deferred hybrid

Many AAA engines run:

```
Deferred (opaque)
Forward+ (transparent)
```

You could support hybrid later.

---

# Final evaluation of your renderer

Your current Forward+ already has:

```
clustered lighting
GPU culling
instancing
shader variants
IBL integration
reflection probes
frame graph
```

That is **already modern architecture**.

With the improvements above you reach **AAA renderer level**.

---

# Most important upgrades (priority order)

If you only do **5 things**, do these:

1. **GPU-only clustered light culling**
2. **Compact cluster light lists (offset + count)**
3. **32 depth slices**
4. **Parallax-corrected reflection probes**
5. **PCF / PCSS shadow filtering**

Those give the **largest real improvement**.

---

To scale **Forward+ to thousands of lights**, engines like Unreal Engine and Frostbite rely on several specific optimizations. Your pipeline already has the base (clustered Forward+), so these are the **next-level techniques**.

---

# 1. Two-stage light culling (very important)

Instead of testing **every light against every cluster**, AAA engines do a **coarse → fine culling**.

### Stage 1 — Screen tiles

Compute lights affecting **screen tiles** (2D).

Example tile grid:

```text
16×16 pixels per tile
```

Output:

```text
Tile → list of lights
```

### Stage 2 — Depth clusters

Only those tile lights are tested against **depth slices**.

```text
Tile lights → test against depth slices → cluster lights
```

This reduces work dramatically.

Instead of:

```text
clusters × lights
```

you get:

```text
clusters × tileLights
```

which is far smaller.

---

# 2. Light volume bounds

AAA engines test lights using **analytic bounding volumes**.

Example:

```text
point light → sphere
spot light → cone
```

Cluster intersection test:

```cpp
Sphere vs ClusterAABB
Cone vs ClusterAABB
```

Cheap math → no expensive per-pixel checks.

---

# 3. GPU prefix-sum light lists

Instead of fixed arrays like:

```text
255 lights per cluster
```

AAA engines use **prefix sum compaction**.

Structure:

```cpp
ClusterHeader
{
    uint offset;
    uint count;
}
```

Global list:

```cpp
uint lightIndices[];
```

So memory looks like:

```text
Cluster0 → [0..5]
Cluster1 → [6..8]
Cluster2 → [9..12]
```

Benefits:

```text
no wasted memory
faster traversal
better GPU cache
```

---

# 4. Wave-friendly compute shaders

Light culling compute shaders are designed for **GPU warp efficiency**.

Typical pattern:

```text
1 thread = 1 cluster
```

Thread checks lights:

```cpp
for(light : lights)
    if(intersect(light, cluster))
        append
```

But optimized using **shared memory**:

```text
load lights into shared memory
all threads test simultaneously
```

This reduces global memory traffic.

---

# 5. Light LOD system

AAA engines don't treat all lights equally.

They use **importance reduction**.

Example rules:

```text
far lights → cheaper shading
small lights → simplified BRDF
very far lights → IBL only
```

This dramatically reduces fragment work.

---

# 6. Light range culling

Before cluster pass, remove lights that cannot affect the camera.

Example checks:

```cpp
if(light outside camera frustum)
    discard
```

Also remove lights whose radius does not intersect the camera depth range.

This prevents useless GPU work.

---

# 7. Shadow-casting light limits

Thousands of lights are possible only because **most do not cast shadows**.

Typical AAA limits:

```text
shadow lights = 4–8
non-shadow lights = hundreds or thousands
```

Shadow maps are the expensive part.

---

# 8. Light shading simplification

Lights in clusters often use simplified equations.

Example:

```text
full PBR → for nearest lights
approximate BRDF → for distant lights
```

This is sometimes called **light shading tiers**.

---

# 9. Specular anti-aliasing

When thousands of lights are present, specular aliasing becomes severe.

AAA engines apply:

```text
normal variance filtering
roughness remapping
```

to stabilize specular highlights.

---

# 10. GPU-driven rendering

Modern engines avoid CPU draw submission entirely.

They use:

```text
GPU scene
indirect draw
compute visibility
```

Lights and objects are handled entirely on GPU.

Your batching system is already close.

---

# 11. Cluster frustum computation optimization

Cluster bounds are computed in **view space** once.

Instead of rebuilding per frame:

```text
reuse cluster planes
```

Only depth slices update if near/far changes.

This saves CPU and GPU time.

---

# 12. Depth slicing improvement

Use **logarithmic depth distribution** (which you already have).

Better values:

```text
tile = 16×16
depth slices = 32
```

This balances near/far light density.

---

# 13. Stencil light rejection (optional)

Some engines mark clusters using **stencil buffers** to reject lights faster.

This is less common now but still useful in some pipelines.

---

# 14. Memory layout optimization

Align light structs for GPU cache.

Example point light:

```cpp
struct PointLight
{
    vec3 position;
    float radius;

    vec3 color;
    float intensity;
};
```

Keep size aligned to **32 or 64 bytes**.

---

# 15. Asynchronous compute

Cluster culling can run on **async compute queue** while graphics renders shadow maps.

Example pipeline:

```text
Compute queue → light culling
Graphics queue → shadow maps
```

Both run simultaneously.

---

# What your engine already does correctly

Your pipeline already includes many good decisions:

```text
clustered lighting
GPU compute culling
SSBO light buffers
instanced batching
IBL integration
reflection probes
frame graph
```

That means your architecture is **modern and scalable**.

---

# If you want thousands of lights, implement these first

Priority order:

1. **Compact cluster lists (offset + count)**
2. **Two-stage tile → cluster culling**
3. **GPU prefix-sum compaction**
4. **32 depth slices**
5. **importance-based light LOD**

Those alone can scale scenes to **1000+ dynamic lights**.

---

# Implemented vs To do (tracking)

Every improvement from this document is listed below. Update the **Implemented** table when work is done; keep **To do** in priority order.

## Implemented

| # | Item | Status / notes |
|---|------|----------------|
| **Core pipeline** | | |
| — | Clustered lighting (16×16 tiles, 32 depth slices, log distribution) | Done. `ClusteredLightGrid`: TILE_SIZE 16, DEPTH_SLICES 32, log near/far. |
| — | GPU compute light culling (default when supported) | Done. `LightCullPass` when supported; pipeline prefers GPU path; counter cleared before dispatch. |
| — | CPU light culling fallback | Done. `ClusteredLightGrid::BuildLightGrid` + `UploadToGPU(m_lightIndexBuffer)` when GPU cull not used. |
| — | Compact cluster storage (offset + count, max 64 per cluster) | Done. Headers + packed indices in CPU (`m_lightGridHeaders`, `m_lightGridPacked`) and GPU (light_cull.comp); buffer sized via `GetLightIndexDataSize()`. |
| — | Forward+ frag shaders read compact layout | Done. `forwardplus*.frag`: baseHeaders, clusterOffset/count, baseIndices; loop over `clusterCount`. |
| — | SSBO light buffers (point binding 3, spot binding 4, light indices binding 2) | Done. `m_pointLightBuffer`, `m_spotLightBuffer`, `m_lightIndexBuffer`; scene lighting binding 5. |
| — | Instanced batching by (mesh, material, submesh) | Done. `BuildBatches(visibleOpaque)`; instanced draw when batch has multiple transforms. |
| — | Shader variants (ForwardPlus static, skinned, instanced) | Done. `ShaderKey{ ForwardPlus, InstanceType, MaterialShaderId }`; templates `forwardplus`, `forwardplus_skinned`, `forwardplus_instanced`. |
| — | Frame graph (Shadow → Forward+ Opaque → Transparency → Skybox) | Done. `BuildForwardPlusGraph`; single HDR output. |
| — | IBL integration (prefiltered cubemap, BRDF LUT, roughness mip) | Done. Forward+ binds irradiance/prefilter/BRDF at 11/12/13; `u_UseIBL`; specular uses `textureLod(prefilter, R, roughness * MAX_LOD)`. |
| — | Reflection probes in pipeline (SSR → probe → skybox) | Done. `FindClosestProbe`/GetGlobalProbe; probe or IBL bound at 11/12/13 for Forward+. |
| — | Material parameters in UBO (MaterialBlock std140, binding 2) | Done. One UBO per material bound per batch; `ApplyToShader(..., cmd)` binds at 2. |
| **Light culling / bounds** | | |
| — | Light volume bounds (sphere vs AABB, cone vs AABB) | Done (CPU path). `ClusteredLightGrid`: `SphereIntersectsAABB`, `ConeIntersectsAABB`; `CullLightsForCluster` uses them. |
| **Shadows** | | |
| — | Directional shadow map (CSM) for Forward+ | Done. Shadow pass in frame graph; `u_ShadowMap` bound; `shadowFactor()` in shader. |
| **Future / hybrid** | | |
| — | (No tiled deferred hybrid yet; doc lists as future) | Not implemented; left as optional later. |

## To do

Items in priority order (high impact first). Tick off by moving rows to **Implemented** and updating notes.

| Priority | # | Item | Notes |
|----------|---|------|--------|
| **Top 5 (doc)** | 1 | Parallax-corrected reflection probes | Correct probe sampling for indoor/boxed environments (position/box influence). |
| | 2 | PCF / PCSS shadow filtering | PCF first; then PCSS soft shadows; contact shadows optional. |
| **Light culling / scalability** | 3 | Two-stage light culling (tile → then depth) | Stage 1: tile → list of lights (2D). Stage 2: only tile lights vs depth slices. Reduces clusters×lights to clusters×tileLights. |
| | 4 | GPU prefix-sum compaction | Build compact light lists with offset+count via prefix sum (same as compact storage; GPU implementation). |
| | 5 | Wave-friendly compute kernels | Optimize LightCullPass: e.g. 1 thread = 1 cluster; shared memory for lights; reduce divergence. |
| | 6 | Light range / frustum culling | Before cluster pass: reject lights outside camera frustum or whose range doesn’t intersect depth range. |
| | 7 | Light importance sorting | On overflow: prioritize closest, brightest, shadow-casting lights. |
| | 8 | Cluster frustum reuse | Reuse cluster planes when possible; only update depth slices when near/far change. |
| **Batching / submission** | 9 | Batching key (mesh, materialShaderVariant) | Refine to materialShaderVariant and push per-instance via instance buffers; reduce binds/pipeline changes. |
| **Shadows** | 10 | Shadow-casting light limits | Document/enforce limit (e.g. 4–8 shadow lights); rest non-shadow. |
| | 11 | VSM / EVSM shadows | Optional later (after PCF/PCSS). |
| **Shading / quality** | 12 | Light LOD / shading tiers | Far lights: cheaper BRDF or IBL-only; nearest: full PBR. |
| | 13 | Specular anti-aliasing | Normal variance filtering, roughness remapping for many lights. |
| | 14 | TAA | Temporal AA for forward pipeline (specular aliasing, shimmering). |
| **Reflection** | 15 | (Reflection stack order SSR → probe → IBL) | Already correct; no change. |
| **Lights** | 16 | Area / tube / rect lights | New light types: data, culling (vs AABB), and shading. |
| **Memory / layout** | 17 | Optimize memory layout | Align PointLight/SpotLight to 32 or 64 bytes for GPU cache; avoid odd sizes. |
| **Optional / later** | 18 | Bindless textures | Material holds texture indices; bindless sample in shader; fewer state changes. |
| | 19 | Depth prepass | Optional for large/dense scenes; early Z, better overdraw. |
| | 20 | Half-resolution lighting | Compute lighting at half res and upscale; mobile/VR. |
| | 21 | Volumetric lighting | Volumetric fog, light shafts, god rays. |
| | 22 | Stencil light rejection | Optional; stencil to reject lights per cluster. |
| | 23 | Asynchronous compute | Run light culling on async compute while graphics does shadow maps. |
| | 28 | GPU-driven rendering | GPU scene, indirect draw, compute visibility (batching already close). |
| | 25 | Tiled deferred hybrid | Deferred opaque + Forward+ transparent; future. |

---

