# Render Fix Track: Professional Spot & Directional Shadows

This document tracks the prioritized fixes and upgrades to resolve shadow artifacts (acne, stripping) and achieve industry-standard shadow fidelity in the OpenEye engine.

## Current Issue: "Stripped" Shadows (Shadow Acne)
**Symptoms:** Striped/moirï¿½ patterns on flat surfaces and at grazing angles.
**Root Cause:** Lack of **Normal Offset Bias** in the fragment shader sampling logic. The depth comparison flip-flops across the surface due to precision and discretization.

---

## implementation order (Professional Track)

### Phase 1: Stability & Artifact Removal (Immediate)
| ID  | Step | Description | Status |
| :-- | :--- | :---------- | :----- |
| T1.1 | **Normal Offset Bias** | Implement vertex-normal-driven position nudging in `deferred_lighting_analytic.frag`. | [x] |
| T1.2 | **Slope-Scaled Bias Tuning** | Adjust RHI-side hardware depth bias factors in `SpotShadowPass` and `DirectionalShadowPass`. | [ ] |
| T1.3 | **Chebyshev Epsilon Tuning** | Refine the variance epsilon to reduce noise in high-variance areas. | [ ] |

### Phase 2: Softness & Fidelity (Premium Quality)
| ID  | Step | Description | Status |
| :-- | :--- | :---------- | :----- |
| T2.1 | **VSM Gaussian Blur Pass** | Add a dual-pass (Horizontal/Vertical) blur pass for shadow atlases to make them "flat and solid". | [ ] |
| T2.2 | **R32G32 Precision Tier** | Switch atlas to 32-bit floats for "High" settings to eliminate moment-precision banding. | [ ] |
| T2.3 | **Light Leak Suppression** | Expose and tune the `linstep` suppression constants for better "solid" shadow centers. | [ ] |

### Phase 3: Advanced Industry Methods (Unreal/Unity Parity)
| ID  | Step | Description | Status |
| :-- | :--- | :---------- | :----- |
| T3.1 | **Poisson Disk Sampling (PCF Path)** | Implement an alternative PCF sampling path with jittered Poisson disks for sharper contact. | [ ] |
| T3.2 | **Temporal Shadow Filtering** | Integrate shadow accumulation with the TAA history (if available) to stabilize shimmers. | [ ] |
| T3.3 | **Contact Shadows** | Add a screen-space ray-march pass to catch fine-detail sub-texel shadows. | [ ] |

---

## Discipline & Rules (Per `Render_steps_todo.md` P7)

1. **Folder Integrity:**
   - Blur shaders go into `assets/shaders/rendering/vulkan/post/`.
   - Blur pass logic goes into `features/shadows/ShadowBlurPass.cpp`.
2. **Blackboard Usage:**
   - The Blur pass reads from `SpotShadowMapAtlas` and writes to a new `BlurredSpotShadowMapAtlas` slot.
   - The Deferred Lighting pass must be updated to consume the *blurred* variant if enabled.
3. **No Execute-Time Creation:**
   - All blur pipelines must be warmed up in `ShadowFeature::OnRegister`.
4. **Professional Identifiers:**
   - Use `uShadow.uNormalBias` instead of magic numbers.
   - Use `shadow_vsm_blur_h` / `shadow_vsm_blur_v` pass names.

---

## Progress Log

*   **2026-05-09:** Initial track created to address "stripped" shadow regression. Root cause identified as missing shader-side normal offset bias.
