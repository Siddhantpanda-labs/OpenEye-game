# OpenGL Refactor — Resource-Space Parity with the Binding ABI

This document is the **single source of truth** for bringing the **OpenGL** backend and **OpenGL GLSL** in line with the same **resource-space contract** as Vulkan: `EngineSource/eyelib/render/rhi/Vulkan/descriptor_binding_table.md` (`OpenEye.ResourceSpaces` **v1**). The goal is **one ABI**, two implementations (Vulkan SPIR-V + OpenGL GLSL), **no parallel “legacy bind” story** for passes that are already migrated on Vulkan.

**Related docs:**

| Doc | Role |
|-----|------|
| `EngineSource/eyelib/render/rhi/Vulkan/descriptor_binding_table.md` | Authoritative binding table (sets, pass kinds, push policy). |
| `documentation/refractor/Render_refractor.md` | Broader render architecture (coordinators, frame data, mode unification). |
| `documentation/refractor/Pipeline_efficiency_roadmap.md` | FPS / compile-once / instancing roadmap. |

---

## 1. Goal (Proper Refactor)

**End state:**

1. **ABI parity** — Every OpenGL shader that participates in resource-space rendering uses `layout(binding = …)` values that are **derivable from** the same logical bindings as the table: per `PipelineLayoutKind`, logical binding indices match what C++ writes via `IDescriptorSet::Update*` after `GLBindingForResourceSet` (see `OpenGLResourceSpaceBindings.cpp`). No ad-hoc texture units that only match the fallback branch.

2. **Single CPU path** — Pass code (`RenderPipeline`, `LightingPass`, `GBufferPass`, post passes, etc.) **does not** maintain a second path that uses `BindTexture` / `BindStorageBuffer` / `Shader::SetInt("u_…", unit)` for the same pass when resource sets exist. Optional: narrow **explicit** fallbacks only when `supportsResourceSpaceBindPath` or required caps are false, with a **defined** outcome (skip pass, reduced quality), not silent wrong images.

3. **Push constants** — OpenGL has no push constants; the contract is satisfied by **UBOs or `uniform`** fields in GLSL, with the **same fields** as Vulkan push blocks for that pass. `CommandListGL::SetPushConstants` may remain a no-op if all such data is supplied via descriptors or shader uniforms filled by the same semantic values as Vulkan.

4. **Capabilities** — `RHICapabilities` (e.g. bindless, resource-space path) gate features; when a path **requires** bindless + descriptor sets, failure is **explicit** (warn once, skip draw), not a hidden legacy path.

5. **Stereo and edge passes** — `forward_stereo` and any remaining **uniform-only** shaders are either migrated to the same binding model as monoscopic forward or **clearly** documented as exceptions with a sunset plan.

6. **Validation** — Where feasible, the same checks described in `descriptor_binding_table.md` (reflection / descriptor writes / bind completeness) are applied or mirrored for GL (e.g. assert logical binding counts per `ResourceSetType`).

---

## 2. Current State (Summary)

| Area | Status |
|------|--------|
| **RHI** | `DeviceGL`, `DescriptorSetGL`, `CommandListGL::BindDescriptorSets`, `GLBindingForResourceSet`, bindless heap when `GL_ARB_bindless_texture` is present. |
| **Forward opaque / Forward+ batched** | Single descriptor + bindless path when readiness checks pass; legacy fallback removed (`ForwardPbrResourceBindingsReady`). |
| **Deferred lighting (`LightingPass`)** | Descriptor-only: Frame + DeferredLightingPass sets; `lighting.frag` bindings aligned with `GLBindingForResourceSet` + table. |
| **GBuffer (`GBufferPass::RenderDrawables`)** | Descriptor + bindless only; `GBufferResourceBindingsReady` gates draws (one-time warn if skipping). Legacy uniform/material buffer + texture-unit path removed. `RenderInstanced`/indirect still use older uniform/bind until migrated. |
| **Stereo forward** | Still **Shader uniform + manual** binds; not on unified resource-space path. |
| **Push constants** | Not native on GL; forward fragments use `uniform`/`UBO` where Vulkan uses push blocks — acceptable if semantics match. |

---

## 3. Non-Goals (for This Refactor)

- Replacing OpenGL with Vulkan on any platform (orthogonal).
- Rewriting the entire renderer; changes should be **pass-by-pass** and consistent with `Render_refractor.md` phases where they overlap.

---

## 4. Phases (Recommended Order)

### Phase A — Binding audit: deferred lighting (OpenGL)

**Goal:** One story: `m_deferredLightingPassSet` updates + `BindDescriptorSets` + **one** `opengl/passes/deferred/lighting.frag` whose `layout(binding=)` lines match **logical** bindings after `GLBindingForResourceSet` for `ResourceSetType::DeferredLightingPass` (same numbering as Vulkan `lighting.frag` for that pass).

**Tasks:**

1. List every `UpdateTexture` / `UpdateStorageBuffer` / `UpdateUniformBuffer` index for the deferred lighting pass set in `RenderPipeline.cpp` (and any pass helper).
2. Compare with Vulkan `passes/deferred/lighting.frag` set 1 bindings.
3. Adjust **either** `GLBindingForResourceSet` **or** `opengl/.../lighting.frag` so **CPU writes ↔ shader bindings** match 1:1.
4. Remove or gate the **legacy** branch in `LightingPass::Render` once GL is proven on the descriptor path only.

**Success criteria:** Deferred lighting looks correct on OpenGL with **only** `BindDescriptorSets(0, 2, sets)` for frame + deferred pass (no manual `BindTexture` for GBuffer/shadow/IBL in the hot path).

---

### Phase B — Remove remaining dual paths in passes (OpenGL)

**Goal:** Any pass that still has `if (useResourceSpace) … else …` manual binds is reduced to **one** path, or a **capability-only** fallback with documented behavior.

**Files to review:** `LightingPass.cpp`, `PostProcessor.cpp`, `GBufferPass.cpp`, `RenderPipeline.cpp` (stereo, outline, skybox).

---

### Phase C — Stereo forward (OpenGL + optional Vulkan alignment)

**Goal:** `RenderForwardOpaqueStereo` uses the same descriptor sets + bindless rules as monoscopic forward **or** a dedicated minimal pipeline that still obeys the binding table for its `PipelineLayoutKind`.

**Tasks:** Update `forward_stereo.vert` / `.frag` (OpenGL) and C++ to bind frame + pass + material + bindless + scene buffers consistently; remove per-draw uniform flood where replaced by SSBO + material UBO.

---

### Phase D — `GLBindingForResourceSet` completeness

**Goal:** Every `ResourceSetType` × `BindingType` × logical index used in production is implemented or asserted; no silent `return logical` that breaks parity for a pass kind.

**File:** `OpenGLResourceSpaceBindings.cpp` (+ unit comments pointing to `descriptor_binding_table.md` sections).

---

### Phase E — Documentation and ABI version

**Goal:** Any change to GL flat bindings or table requires the same process as `descriptor_binding_table.md` (version bump, synchronized shader + C++ + layout).

---

## 5. Implementation Order Summary

| Order | Phase | Main outcome |
|-------|--------|----------------|
| A | Deferred lighting audit | GL `lighting.frag` + `GLBindingForResourceSet` + `LightingPass` match Vulkan logical bindings; legacy branch removable. |
| B | Dual-path removal | Passes use one descriptor binding path or explicit capability failure. |
| C | Stereo | Stereo forward aligned with resource-space model or documented exception removed. |
| D | Mapping completeness | No silent wrong binding for any pass kind. |
| E | Process | Table + GL stay locked. |

---

## Progress tracker

Update this section when phases complete or scope changes. Dates are optional.

| Phase | Status | Notes |
|-------|--------|--------|
| A — Deferred lighting binding audit | Done | `GLBindingForResourceSet` maps DeferredLightingPass (textures 2–6, SSBO 7, LightSpace UBO 8, IBL 9–11); `opengl/.../lighting.frag` uses Frame UBO 0 + physical bindings; `LightingPass` is descriptor-only (no `BindForLightingPass` / manual texture path). |
| B — Dual-path removal (passes) | In progress | **G-buffer drawables** + lighting done; **PostProcessor** (many `BindTexture` paths), **skybox**, **stereo**, **GBuffer `RenderInstanced`**, indirect draws still legacy or mixed. |
| C — Stereo forward | Not started | Still uniform + manual binds in `RenderForwardOpaqueStereo`. |
| D — `GLBindingForResourceSet` completeness | Not started | Audit all `ResourceSetType` usages vs table. |
| E — ABI + doc process | Not started | |

**Completed prior to this doc (baseline):**

| Item | Status | Notes |
|------|--------|--------|
| Forward opaque + Forward+ batched (single descriptor path) | Done | No legacy `ApplyToShader` / manual global bind fallback; `ForwardPbrResourceBindingsReady` gates draws. |
| `DeviceGL::supportsResourceSpaceBindPath` | Done | Enables resource-set layout path. |
| `descriptor_binding_table.md` | Living doc | Vulkan contract; GL must mirror semantics. |

---

*Document: OpenGL refactor — resource-space parity.*  
*Location: `documentation/refractor/opengl_refractor.md`.*
