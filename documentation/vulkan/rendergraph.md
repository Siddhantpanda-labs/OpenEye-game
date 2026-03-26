Alright — compressing it to the **core mental model used in Unreal / modern engines**, no fluff.

---

# 🧠 1. Unreal-style architecture (the real idea)

> **CPU prepares once → GPU consumes many times → shaders decide behavior**

---

## 🔷 Data is split by frequency (THIS is the key)

### ✔️ Frame (updated once per frame)

```cpp
FrameUniforms
- view / projection
- camera position
- time
```

👉 Updated once
👉 Reused across *all passes*

---

### ✔️ Scene (rarely changes)

```cpp
SceneData
- lights
- environment
- global textures
```

👉 Updated only when needed
👉 Treated as **read-only during rendering**

---

### ✔️ Material / Object (bound per draw)

```cpp
Material / Object data
- textures
- parameters
- transforms
```

👉 Bound per draw call
👉 No mutation during pass execution

---

## 🔥 Critical rule Unreal follows

> **No descriptor mutation during rendering**

Everything is:

* Pre-built
* Immutable
* Just *bound and used*

---

## ⚡ Where behavior changes?

Not by changing descriptors — but via:

```glsl
// Shader decides
if (isTransparent) { ... }
```

or

* different pipelines
* different shaders
* different PSOs

👉 CPU does NOT rewrite sets per pass

---

# 🚨 Your misunderstanding (fix this)

You were trying:

> “Change descriptors between passes”

Unreal does:

> “Bind different prebuilt states — never rewrite”

---

# 🧠 2. Industry Render Graph (real AAA model)

This is the real structure behind modern engines.

---

## 🔷 Everything becomes a node

```cpp
OpaquePass
TransparentPass
ShadowPass
PostProcessPass
```

---

## 🔷 And resources are explicit

```cpp
ColorBuffer
DepthBuffer
GBuffer
```

---

## 🔷 Then you define dependencies

```cpp
OpaquePass → writes Color + Depth

TransparentPass → reads Depth, writes Color
```

---

## 🔥 What the render graph does

### ✔️ 1. Builds execution order automatically

No manual sequencing mess.

---

### ✔️ 2. Allocates resources safely

No accidental overwrites.

---

### ✔️ 3. Enforces correctness

You literally **cannot**:

* write + read same resource incorrectly
* mutate shared state illegally

---

## ⚡ What changes for you

Instead of this:

```cpp
RenderOpaque();
ModifyDescriptors();
RenderTransparent();
```

You move to:

```cpp
AddPass(OpaquePass);
AddPass(TransparentPass);
```

And the system handles:

* resource states
* transitions
* ordering

---

# 🧠 3. The real industry mental model

This is the one you should adopt:

```
CPU builds a frame plan (graph)
        ↓
All resources + descriptors are created upfront
        ↓
GPU executes without mutation
        ↓
Shaders define behavior differences
```

---

# 🔥 Brutal truth

If you are:

* modifying descriptor sets mid-frame
* sharing mutable state across passes

👉 You are **pre-render-graph era (old / fragile design)**

---

# ✅ What “perfect architecture” looks like

### ✔️ Immutable bindings

No runtime mutation.

---

### ✔️ Pass isolation

Each pass owns its state.

---

### ✔️ Shader-driven variation

Not CPU-driven descriptor rewriting.

---

### ✔️ Graph-driven execution

Not manual sequencing.

---

# ⚡ One-line summary (burn this in)

> **Build everything once → never mutate → let graph + shaders handle the rest**

---

If you want next step, I can map your current engine into:

👉 a **minimal render graph implementation (not overengineered)**
so you transition cleanly instead of rewriting everything blindly.

---

## Progress (Current Migration)

| Area | Status | Small note |
|---|---|---|
| Vulkan legacy `Shader::Set*` uniform path | Done | Vulkan path no-ops legacy uniform emulation; typed push/resource-space path is primary. |
| `PassPushConstants.h` consolidation | Done | Core forward/deferred/IBL/skybox/entity/SSR/depth + post-effect push writers centralized. |
| PostProcessor immediate path | Done | Post effects use `abi::Write*` helpers instead of ad-hoc `SetPushConstants` blobs. |
| Post frame-graph ToneMap + FXAA | Done | Frame-graph passes now also use post descriptor-set + push helper path. |
| Per-frame forward debug log spam | Done | Repeating `ForwardOpaque*` info logs removed; only suspicious submit case warns. |
| Opaque/transparent descriptor lifetime safety | In progress | Current issue: transparent path can update shared forward sets after opaque uses them. |
| Pass-isolated immutable descriptor sets (Unreal-style) | Left to do | Split/prepare per-pass descriptor instances upfront; no update-after-bind in execute path. |
| Full graph-driven pass parameter model | Left to do | Move to explicit pass params/resources ownership and immutable execute phase. |

> Note: next critical fix is descriptor lifetime isolation for forward opaque vs transparent (same-frame update is fine, update-while-in-use is not).
