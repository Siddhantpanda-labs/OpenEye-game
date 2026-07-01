If Sponza is still dropping to ~6 FPS **after Steps 1–5**, then the bottleneck is no longer the original “multiple snapshot rebuilds per panel” architecture. The problem has moved deeper into the render pipeline.

From your checklist, the biggest missing performance-critical systems are:

* Step 6 — panel render request decoupling
* Step 7 — resource resolution hardening
* Step 8 — render threading
* GPU-side batching/visibility inefficiencies (likely now dominant)

Your current architecture fixes CPU duplication, but Sponza stresses:

* draw-call count,
* material switches,
* descriptor updates,
* visibility cost,
* GPU submission overhead,
* shadow/light passes,
* synchronization stalls.

The fact you are at **6 FPS** strongly suggests one or more of:

1. GPU saturation
2. Render-thread stall on main thread
3. Massive draw-call explosion
4. Per-frame descriptor/material rebinding
5. Shadow map overdraw
6. Visibility system still effectively O(N)
7. Sync/wait-for-idle behavior
8. ImGui/editor compositing cost
9. Vulkan/DX validation accidentally enabled
10. Rebuilding command buffers/material pipelines every frame

---

# Most Likely Issue

Your Step 5 visibility system is probably still not reducing enough work.

Sponza commonly contains:

* 3000–10000 draw submissions
* many materials/submeshes
* many state changes

If you still emit nearly all draws every frame, CPU submission alone can destroy FPS.

---

# What You Need Next (Critical)

## 1. Measure CPU vs GPU FIRST

Before changing architecture further:

Add timings for:

* Scene build
* Visibility
* Draw list generation
* Render submission
* GPU frame time
* Shadow pass
* Main thread wait
* Render thread wait

You need:

```cpp
CPU Frame:
- Update
- Visibility
- BuildDraws
- Submit

GPU:
- Geometry
- Shadows
- Lighting
- Post
```

Without this you are blind.

This is now mandatory. 

---

# Extremely Likely Hidden Killer

## You may still be rebuilding render packets every frame

Even with caching.

Check whether you still do:

```cpp
std::vector<DrawCommand> draws;
for(all visible)
{
    BuildDraw(...);
}
```

every frame.

That still kills performance at Sponza scale.

You need:

* persistent draw packets
* persistent descriptor bindings
* stable GPU-ready draw structures

Visibility should only produce:

```cpp
VisibleIndices[]
```

NOT rebuild full draw commands.

---

# Another Major Suspect: Descriptor Churn

If every draw does:

```cpp
BindMaterial()
UpdateDescriptor()
ResolveTexture()
```

you are dead.

Sponza heavily stresses material switching.

You need:

* persistent descriptor sets
* bindless textures ideally
* material sorting
* PSO sorting
* cached pipeline state

---

# Your Visibility System May Still Be Too Weak

Current:

* frustum vs sphere

Not enough for Sponza.

Sponza is an indoor occlusion-heavy scene.

You need at least one:

* BVH
* portal culling
* Hi-Z occlusion
* software occlusion raster
* clustered visibility

Otherwise you still render almost the entire scene.

---

# Critical Question

How many draw calls are you issuing in Sponza?

You should measure:

* total draws
* visible draws
* culled draws
* shadow draws

If you are above ~2000–4000 draws without aggressive batching/instancing/threading, 6 FPS is plausible.

---

# Another Common Catastrophic Bug

## Waiting for GPU every frame

Check for:

```cpp
vkDeviceWaitIdle()
vkQueueWaitIdle()
glFinish()
```

inside frame loop.

If present:
instant catastrophic FPS collapse.

Especially common during:

* resize
* editor viewport
* texture uploads
* descriptor rebuilds

---

# Editor-Specific Problem

You are rendering:

* SceneView
* GameView
* gizmos
* editor UI
* maybe thumbnails/previews

even if architecture improved.

Check if Sponza is effectively rendered:

* twice
* three times
* once per docked panel

This is VERY common in editor engines.

---

# Highest-Value Next Step

## Implement Step 9 BEFORE Step 8

You need hard telemetry now.

Add:

```cpp
struct RenderStats
{
    uint32_t totalDraws;
    uint32_t visibleDraws;
    uint32_t culledDraws;
    uint32_t materialBinds;
    uint32_t pipelineBinds;

    double snapshotMs;
    double visibilityMs;
    double drawBuildMs;
    double submitMs;
    double gpuFrameMs;
};
```

Then overlay it in editor.

Without this, further optimization becomes guesswork. 

---

# Expected Performance

With your current architecture:

### If optimized correctly:

Sponza should roughly achieve:

| Renderer State        | Expected FPS |
| --------------------- | ------------ |
| naive editor renderer | 5–15 FPS     |
| cached + visibility   | 20–60 FPS    |
| threaded + batched    | 60–144 FPS   |
| bindless + indirect   | 100+ FPS     |

So 6 FPS means:
you still have a major systemic bottleneck.

---

# What I Would Investigate Immediately

Priority order:

1. GPU wait stalls
2. Draw-call count
3. Multiple viewport rendering
4. Descriptor/material rebinding
5. Shadow pass explosion
6. Full draw packet rebuilds
7. Validation layers enabled
8. Command buffer recreation
9. Visibility effectiveness
10. Lack of batching

---

# Very Important

Your current steps mostly optimize:

* scene extraction
* ECS traversal
* snapshot duplication

But Sponza performance is usually dominated by:

* render submission architecture
* GPU state churn
* visibility quality

So the bottleneck has likely shifted from ECS → renderer now.

That is actually progress.
