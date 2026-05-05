# Swapchain, HDR, and present policy (Vulkan RHI)

Companion to `documentation/vulkan/steps_todo.md` **Phase 8**. Describes **runtime behavior** implemented in `DeviceVK`, `DeviceVKPresentation`, and `SwapchainVK`.

---

## 1. `VK_ERROR_OUT_OF_DATE_KHR` vs `VK_SUBOPTIMAL_KHR`

| Result | Meaning (WSI) | Engine policy |
|--------|----------------|-----------------|
| `VK_ERROR_OUT_OF_DATE_KHR` | Swapchain is no longer compatible with the surface (resize, mode change, focus, etc.). | **`DeviceVK::NoteSwapchainKhrResult`** calls **`SwapchainVK::Resize(..., forceRecreate=true, ...)`** when device, surface, and swapchain handles are valid. Used from **`BeginFrame`** (`vkAcquireNextImageKHR`) and **`Present`** (`vkQueuePresentKHR`). |
| `VK_SUBOPTIMAL_KHR` | Image index is valid; present/acquire succeeded but the swapchain could be recreated for a better match (e.g. present mode vs compositor). | **No automatic resize.** Capped **Info** on acquire; one-shot **Warn** on present. Call **`Resize`** from app/window path if you need to match surface settings. |

Centralization: **`DeviceVK::NoteSwapchainKhrResult`** (`DeviceVK.cpp`) owns logging + **`OUT_OF_DATE`** → **`Resize`** so acquire and present stay consistent.

---

## 2. `vkDeviceWaitIdle` on swapchain recreate

**`SwapchainVK::Resize`** begins with **`vkDeviceWaitIdle`** before destroying framebuffers, render passes, image views, and the old **`VkSwapchainKHR`**.

This is intentional for **Phase 8**: teardown must not race in-flight command buffers or present semaphores. Removing the wait would require a narrower GPU-idle contract (per-queue idle, timeline points, or deferred destroy) and is **not** attempted here.

**`OnWindowResized`** calls **`Resize(..., forceRecreate=false, ...)`** — same wait when dimensions change.

---

## 3. HDR / `SwapchainOutputMode` and fallbacks

Selection lives in **`SwapchainVK::SelectSurfaceFormat`** (`SwapchainVK.cpp`).

- **`SdrSrgb`** or missing **`VK_EXT_swapchain_colorspace`**: prefer **`B8G8R8A8_UNORM`** + **`VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`**; else first listed format.
- **`Hdr10St2084`**: prefer **`A2B10G10R10_UNORM_PACK32`** + **`VK_COLOR_SPACE_HDR10_ST2084_EXT`**, then any format with that color space. If none: **Warn** and fall back to SDR selection.
- **`HdrExtendedSrgbLinear`**: prefer **`R16G16B16A16_SFLOAT`** + **`VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT`**, then any format with that color space. If none: **Warn** and fall back to SDR.

**`DeviceVK::SetSwapchainOutputMode`** triggers **`Resize`** so mode changes apply on the next recreate.

**Manual verification:** OS HDR toggle, monitor EDID, and compositor behavior are outside the RHI; use **`GetSwapchainSurfaceColorInfo`** after resize to confirm the resolved **`VkFormat`** / **`VkColorSpaceKHR`**.

---

## 4. Present timing (optional pacing)

When **`VK_KHR_present_id`** and **`VK_KHR_present_wait`** are enabled, **`SwapchainVK::Present`** assigns monotonic present IDs and the device exposes **`WaitForPresent`** / **`GetLastPresentId`**.

**`IDevice::WaitForPreviousPresent`** is an **opt-in** frame-loop helper: it does **not** run inside **`BeginFrame`** or **`Present`**.

Recommended pattern for a latency budget tied to a nominal refresh rate:

1. Query **`SupportsPresentWait()`** (or **`RHICapabilities::supportsPresentWait`**).
2. After **`Present()`** (or at a known point in the frame), call **`WaitForPreviousPresent`** with **`rhi::PresentWaitTimeoutNsFromTargetHz(targetHz)`** from **`RHITypes.h`** (`0` Hz → **`UINT64_MAX`**, i.e. no timeout cap).

See **`documentation/vulkan/vulkan_check.md`** changelog (present pacing / Phase D) and **`DeviceVK.h`** frame invariants comment.
