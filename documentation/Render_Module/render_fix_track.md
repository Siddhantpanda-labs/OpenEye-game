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
| T1.3 | **Chebyshev Epsilon Tuning** | Refine directional/spot VSM variance epsilon and light-leak suppression windows to reduce striping/noise near contact surfaces. | [x] |
| T1.4 | **Spot shadow layer indexing correctness** | Use compact shadow-caster layer indices (no sparse/light-index mapping), share exact same spot view-proj matrices between UBO and shadow pass, and resize spot shadow atlas when caster count changes. | [x] |

### Phase 2: Softness & Fidelity (Premium Quality)
| ID  | Step | Description | Status |
| :-- | :--- | :---------- | :----- |
| T2.1 | **VSM Gaussian Blur Pass** | Add a dual-pass (Horizontal/Vertical) blur pass for shadow atlases to make them "flat and solid". | [ ] |
| T2.2 | **R32G32 Precision Tier** | Switch atlas to 32-bit floats for "High" settings to eliminate moment-precision banding. | [ ] |
| T2.3 | **Light Leak Suppression** | Expose and tune the `linstep` suppression constants for better "solid" shadow centers. | [x] |

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
*   **2026-05-09:** Spot light visibility fix landed: disabled unstable blur-pass scheduling path and corrected spot shadow layer mapping in `ShadowFeature` (compact caster-layer indexing + shared view-proj mapping + dynamic layer-count RT reallocation). This removed the "light present but shadowed out" failure mode caused by invalid/sparse atlas layer addressing.
*   **2026-05-09:** VSM quality tuning pass landed in deferred shader: added shared `ComputeVsmVisibility`, applied receiver depth bias in VSM compare depth, and tuned separate variance/leak-suppression ranges for directional vs spot shadow sampling to reduce near-surface striping and stabilize contact response.
*   **2026-05-09:** Solid-shadow clamp pass landed: directional/spot VSM visibility is now clamped with a depth-only 3x3 PCF term (`min(VSM, PCF)`) to eliminate faded/striped translucent shadows and restore dark, stable shadow cores while keeping soft transitions.
*   **2026-05-09:** Lit-side striping mitigation landed: replaced global hard `min(VSM,PCF)` behavior with contact-weighted blending (`mix(VSM, min(VSM,PCF), contactWeight)`) and added derivative+slope-aware receiver depth stabilization in deferred shadow sampling. This targets rotating-cube slit artifacts and jagged bright-side striping in directional/Sponza lighting.
*   **2026-05-09:** Spot-shadow regression correction: reduced spot receiver stabilization aggressiveness (lower slope/derivative scaling), softened spot VSM suppression constants, and tightened contact-weighted clamp window to restore coherent spot shadow shape while keeping directional striping mitigation active.
*   **2026-05-09:** Spot method switch completed: removed VSM from spot shadows and migrated to depth-only shadow map + PCF sampling path. `ShadowFeature` now builds a dedicated depth-only spot shadow pipeline and publishes spot depth array for deferred sampling (`SampleSpotShadowPCF`), improving stability and avoiding moment-based striping/regression.
*   **2026-05-09:** Contact grounding refinement: calibrated spot-specific caster/receiver bias to professional depth+PCF defaults (lower spot hardware slope bias in `SpotShadowPass`, reduced spot normal-offset and receiver-depth bias in deferred shader) to eliminate near-surface bright inner circles while preserving acne resistance.
*   **2026-05-09:** Directional stability/acne calibration pass: reduced directional camera-dependent receiver derivative influence, narrowed directional contact clamp range, and retuned directional caster/receiver bias pair (`DirectionalShadowPass` hardware bias + `ShadowParamsStd140` defaults) to remove lit-side jagged striping and reduce camera-rotation shadow spreading in Sponza-scale scenes.
*   **2026-05-09:** Directional method switch completed: removed VSM from directional shadows and migrated to depth-only CSM + PCF path (depth-array RT + `directional_depth` pipeline + `SampleDirectionalShadowPCF`). Updated directional shadow resource state handling to `DepthRead` in deferred pass. This is the final stable baseline for directional parity with spot no-variance strategy.
*   **2026-05-09:** Full-flow root-cause refinement: switched shadow bias basis from GBuffer shading normal (normal-mapped, high-frequency) to reconstructed geometric normal from world-position derivatives in deferred shadow sampling. This removes texture-driven bias jitter that manifested as lit-side jagged strip lines under camera rotation.
*   **2026-05-09:** Directional acne suppression hardening: increased directional caster slope bias and strengthened directional receiver bias budget (base + slope + derivative terms) to suppress persistent lit-side striping on Sponza fabrics/stone under camera rotation while retaining cascade stability and spot-shadow behavior.
*   **2026-05-09:** Directional full-flow correction: removed directional receiver derivative bias term (major source of distance flicker), made directional bias explicitly cascade-aware (base+normal bias scale with cascade index), and re-enabled per-cascade directional PCF radius via `uFilterExtent`. Directional bias basis reverted to stable GBuffer normal (not reconstructed depth-derivative normal) to avoid far-distance camera-dependent shimmer.
