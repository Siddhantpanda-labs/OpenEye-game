# Vulkan present & debug

## Architecture (single-source present)

- One overlay pass (LOAD) for ImGui; null-src path: clear swapchain image then LOAD pass + overlay. Blit path: copy scene then LOAD pass + overlay.
- See repo notes on centralized present flow.

---

## Debug tweaks (black window)

1. **Red clear (is swapchain output visible?)**  
   CMake option: `OPENEYE_VULKAN_DEBUG_CLEAR_RED` (default ON for debug). When ON, null-src path clears to bright red instead of dark gray.
   - **Red window** → present path works; issue is likely ImGui (viewport/size or pipeline).
   - **Still black** → issue is earlier: wrong image, submit/present/sync, or command buffer not used for presented image.
   - Revert: set `OPENEYE_VULKAN_DEBUG_CLEAR_RED` to OFF in CMake and reconfigure to restore normal clear (0.12, 0.12, 0.15, 1).

2. **Logging**  
   DeviceVKPresentation and DeviceVK log once (or first few frames) on skip/fail and on successful clear+overlay path. Editor logs ImGui draw data (TotalIdx/TotalVtx) for first 3 Vulkan frames. **BeginFrame:** if `vkBeginCommandBuffer` fails, an error is logged (`Vulkan BeginFrame: vkBeginCommandBuffer failed (0x%x); frame may be black`); that can explain black when the buffer is never started.

3. **Validation layer**  
   CMake option: `OPENEYE_VULKAN_VALIDATION` (default ON). When ON, instance is created with `VK_LAYER_KHRONOS_validation` and `VK_EXT_debug_utils` (if the layer is available). Validation messages go to stderr. Set OFF to disable. Requires Vulkan SDK installed.

4. **If red shows, next checks**  
   ImGui viewport/display size vs swapchain extent (e.g. 1280×720); ensure main viewport and framebuffer size match so ImGui isn’t drawing at zero size or off-screen.
