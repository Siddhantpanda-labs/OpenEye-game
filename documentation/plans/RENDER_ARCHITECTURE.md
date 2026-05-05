# Render Architecture — Doctrine and Ownership

Short internal reference for render architecture. Use in reviews to prevent regression.

---

## 1. Six-point doctrine

1. **No global mutable render state.** Device and context are passed explicitly; pipelines do not set global device.
2. **Device lifetime ≥ everything GPU-related.** All GPU resources are created with a device and must not outlive it.
3. **Assets are CPU domain.** CPU descriptors (ShaderDescriptor, MeshDescriptor, etc.) are device-independent.
4. **GPU objects are device domain.** Shader, Mesh, Texture2D, Material instances are bound to a device; they live in per-device caches, not in the registry’s canonical asset data.
5. **Short lifetime must not escape upward.** Objects from a shorter lifetime (e.g. play session) must never be stored in a longer-lived cache (e.g. editor registry) unless they are independent of that shorter lifetime’s infrastructure.
6. **Ownership must be explicit.** Who owns the device, who owns GPU resources, and who owns registry entries must be clear; no hidden global mutation.

---

## 2. Resource ownership (GPU)

Every GPU resource is assigned to exactly one of these lifetimes. No ambiguous ownership.

| Lifetime    | Owner / scope                    | Examples |
|------------|-----------------------------------|----------|
| **Device** | RHI device; lives for app/editor  | RHI device, bindless heap, pipeline caches, descriptor pools. |
| **Scene**  | Scene or world load               | Scene-specific render state if any; typically we use device + frame. |
| **Frame**  | Single frame or pass              | Transient command lists (if created per frame), per-frame scratch buffers, RenderContext. |

**Rules:**

- **Device-lifetime:** Created at init, destroyed with the device. Pipeline-owned buffers (e.g. light buffers, cull buffers), GBuffer, shadow map, HDR RT, post-process RTs.
- **Scene-lifetime:** Optional; for resources that are created when a scene loads and released when the scene unloads. Currently most “scene” state is device-backed and keyed by scene in higher-level code.
- **Frame-lifetime:** Not stored beyond the frame; built and consumed in one frame (or one pass). RenderContext is built per frame from the device’s immediate command list.

---

## 3. Registry vs runtime

**Registry (asset identity + CPU representation + metadata):**

- Stores: AssetId, CPU descriptors (ShaderDescriptor, MeshDescriptor, TextureDescriptor, MaterialDescriptor), type names, metadata.
- Does **not** store: GPU handles, device pointers, or any device-bound state.
- Engine-agnostic: usable without an active RHI device.

**GPU instances:**

- Created when “binding” to a device/context (e.g. `GetShader(id)` → instance cache for current device, or create from descriptor).
- Live in **per-device instance caches** (keyed by AssetId + device). Not in the registry’s canonical asset data.
- Legacy `AssetCache` may still hold `shared_ptr` to GPU objects for backward compatibility; the canonical path is descriptor → per-device instance. On registry clear, `ClearDescriptorsAndInstances()` clears instance caches; the legacy cache is cleared separately.

**Rule:** Registry stores asset identity, CPU representation, and metadata only. GPU instances are created from descriptors when resolving for a device/context.

---

## 4. Device loss (§9)

On device lost or driver reset: (1) `AssetManager::Get().OnDeviceLost()` (clears instance caches; descriptors kept). (2) `pipeline->Shutdown()` while device is still valid. (3) Destroy device. (4) Create new device. (5) `SetRenderDevice(newDevice)`. (6) `pipeline->Initialize(newDevice, w, h)`. Assets are re-resolved from descriptors on first `Get*` after the new device is set. See **Phase6_plan.md** for full sequence.

---

## 5. References

- **Current_advice.md** — Full roadmap, phases 1–6, implementation status.
- **Phase6_plan.md** — Phase 6 (frame graph recipe, device loss sequence, threading deferred).
- **Phases 1–4** — Single device (Model A), explicit device, RenderContext, CPU descriptors and per-device instance resolution.
