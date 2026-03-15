Your engine **did start**, but the validation output shows several **real Vulkan correctness violations**. The good news: most are configuration mistakes, not architectural failures.

I'll go through the **important ones in order of severity**, explain what they mean, and what to fix.

---

# 1️⃣ Descriptor Indexing Misuse (first errors)

Error:

```
VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
but descriptor indexing features were not enabled
```

Meaning:

Your descriptor set layout uses flags like:

```
VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
```

but you **never enabled descriptor indexing features** when creating the device.

### Fix Option A (simplest)

Remove these flags.

Your layout likely has something like:

```cpp
VkDescriptorBindingFlags flags[] =
{
    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
};
```

Change to:

```cpp
VkDescriptorBindingFlags flags[] = {0};
```

---

### Fix Option B (if you want bindless textures)

Enable descriptor indexing:

```cpp
VkPhysicalDeviceDescriptorIndexingFeatures indexing{};
indexing.sType =
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

indexing.descriptorBindingPartiallyBound = VK_TRUE;
indexing.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
indexing.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
```

and chain into device creation.

But unless you're implementing **bindless rendering**, just **remove the flags**.

---

# 2️⃣ Timeline Semaphore Not Enabled

Error:

```
semaphoreType is VK_SEMAPHORE_TYPE_TIMELINE
but timelineSemaphore feature was not enabled
```

Meaning you created:

```cpp
VkSemaphoreTypeCreateInfo typeInfo;
typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
```

but never enabled the feature.

### Fix

During device creation:

```cpp
VkPhysicalDeviceTimelineSemaphoreFeatures timeline{};
timeline.sType =
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
timeline.timelineSemaphore = VK_TRUE;
```

chain into:

```
VkDeviceCreateInfo.pNext
```

---

# 3️⃣ Debug Utils Struct Wrong

Error:

```
vkSetDebugUtilsObjectNameEXT(): sType must be
VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT
```

You likely forgot to initialize `sType`.

### Fix

```cpp
VkDebugUtilsObjectNameInfoEXT info{};
info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
info.objectType = VK_OBJECT_TYPE_IMAGE;
info.objectHandle = (uint64_t)image;
info.pObjectName = "ShadowDepth";
```

---

# 4️⃣ Pipeline Created With `renderPass = NULL`

Error:

```
renderPass is NULL but dynamicRendering feature was not enabled
```

Your pipeline creation:

```cpp
pipelineInfo.renderPass = VK_NULL_HANDLE;
```

This only works if **dynamic rendering** is enabled.

### Fix Option A (recommended)

Enable dynamic rendering:

```cpp
VkPhysicalDeviceDynamicRenderingFeatures dynamic{};
dynamic.sType =
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
dynamic.dynamicRendering = VK_TRUE;
```

Chain into device creation.

---

### Fix Option B

Use a real render pass:

```cpp
pipelineInfo.renderPass = myRenderPass;
```

---

# 5️⃣ Shader / Descriptor Layout Mismatch

Example error:

```
shader uses descriptor type STORAGE_BUFFER
but layout expects COMBINED_IMAGE_SAMPLER
```

Meaning:

Your shader contains:

```
layout(set=0,binding=5) buffer u_SceneTexture
```

but your descriptor layout declares:

```
binding 5 = COMBINED_IMAGE_SAMPLER
```

### Fix

Shader and descriptor layout **must match exactly**.

Check:

```
set
binding
descriptor type
array size
```

---

# 6️⃣ Push Constant Range Too Small

Error:

```
push constant block [0,148]
but layout only allows [0,128]
```

Your pipeline layout likely uses:

```cpp
range.size = 128;
```

But shader push constant block is larger.

### Fix

Increase push constant size:

```cpp
range.size = 256;
```

Most GPUs allow **256 bytes**.

---

# 7️⃣ Storage Buffer in Fragment Shader Without Feature

Error:

```
storage buffer used in fragment shader
but fragmentStoresAndAtomics not enabled
```

Fix:

Enable feature:

```cpp
VkPhysicalDeviceFeatures features{};
features.fragmentStoresAndAtomics = VK_TRUE;
```

---

# 8️⃣ Image Layout Wrong (important)

Error:

```
ShadowDepth image transitioned to COLOR_ATTACHMENT_OPTIMAL
but image is DEPTH_STENCIL
```

Your shadow map image was created with:

```
VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
```

but you're transitioning it to:

```
VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
```

### Correct layout:

```
VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
```

or

```
VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
```

---

# 9️⃣ Compute Pipeline Descriptor Mismatch

Error:

```
DrawListIn is UNIFORM_BUFFER
but layout expects STORAGE_BUFFER
```

Again: shader and descriptor layout mismatch.

Fix either shader or descriptor type.

---

# Overall Assessment

Your engine **is structurally correct**, but these issues indicate:

* descriptor layouts not synchronized with shaders
* missing Vulkan features
* push constant size mismatch
* incorrect image layouts

These are **common engine bring-up issues**.

---

# The 4 Fixes That Will Remove 80% of Errors

1️⃣ Enable features during device creation:

```
dynamicRendering
timelineSemaphore
fragmentStoresAndAtomics
```

2️⃣ Increase push constant size to **256**

3️⃣ Fix shadow depth layout transition

```
COLOR_ATTACHMENT_OPTIMAL → DEPTH_STENCIL_ATTACHMENT_OPTIMAL
```

4️⃣ Remove descriptor indexing flags if unused.

---

# Final Honest Assessment

Your engine is **already in the phase where validation errors are real engine bugs, not architecture problems**.

That means you're **past the hardest part**.

Fixing these is mostly **descriptor + feature configuration work**.

---

# Validation run reference (2026-03-15)

From a single editor run with `OPENEYE_VULKAN_VALIDATION=1`:

| Category | VUID | Message (short) |
|----------|------|-----------------|
| Descriptor indexing | VUID-VkDescriptorSetLayoutCreateInfo-flags-03000 | Binding has UPDATE_AFTER_BIND_BIT but layout flags don't include UPDATE_AFTER_BIND_POOL_BIT |
| Descriptor indexing | 03006 / 03008 / 03013 | Sampled image / storage buffer / partially bound: enable corresponding descriptor indexing feature |
| Timeline semaphore | VUID-VkSemaphoreTypeCreateInfo-timelineSemaphore-03252 | timelineSemaphore feature not enabled |
| Debug utils | VUID-VkDebugUtilsObjectNameInfoEXT-sType-sType (0xce0e3443) | sType must be VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT |
| Compute descriptor | VUID-VkComputePipelineCreateInfo-layout-07990 | Shader DrawListIn is UNIFORM_BUFFER but layout expects STORAGE_BUFFER |
| Dynamic rendering | VUID-VkGraphicsPipelineCreateInfo-dynamicRendering-06576 | renderPass is NULL but dynamicRendering not enabled |
| Fragment storage | VUID-RuntimeSpirv-NonWritable-06340 | Storage buffer in fragment without fragmentStoresAndAtomics (or NonWritable) |
| Push constants | VUID-VkGraphicsPipelineCreateInfo-layout-10069 | Push constant range [0,148] or [0,232] outside layout range [0,128] |
| Layout mismatch | 07990 / 07988 / 07987 | Shader descriptor type vs layout; push constant stage not in layout |
| Image barrier | VUID-VkImageMemoryBarrier-oldLayout-01208 (0xf855a75b) | RT_ShadowDepth (depth image) transitioned to COLOR_ATTACHMENT_OPTIMAL — use DEPTH_STENCIL_ATTACHMENT_OPTIMAL |

**Fixes applied in code:** BarrierTracker uses depth layout for depth-aspect images when state is RenderTarget; DebugUtils uses official `VkDebugUtilsObjectNameInfoEXT` and `sType`; device creation enables `fragmentStoresAndAtomics`, descriptor indexing features, `timelineSemaphore`, and `dynamicRendering` via `VkPhysicalDeviceFeatures2` chain. Remaining: descriptor set layout flags (UPDATE_AFTER_BIND_POOL_BIT where needed), push constant sizes, shader/layout descriptor type and stage matches.

---
