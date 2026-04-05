Here’s the **industry-level (Unreal-style) rendering design** — clean, complete, no confusion:

---

# 🔥 1. Frame Pipeline (NO global switch)

```text id="frame_pipeline"
Frame:

1. Shadow Pass
2. GBuffer Pass (Deferred - opaque)
3. Deferred Lighting Pass
4. Forward+ Pass (transparent + special)
5. Post Processing
```

👉 Always this structure
👉 No “choose forward or deferred globally”

---

# 🔹 2. Object Routing (the “branching” you asked)

```text id="object_routing"
if (material.blendMode == Opaque or Masked):
    → GBuffer Pass (Deferred)

if (material.blendMode == Transparent):
    → Forward+ Pass
```

Optional:

```text id="special_cases"
if (material.requiresForward):
    → Forward+ (even if opaque)
```

---

# 🔹 3. Material System (industry clean)

```text id="material_system"
Material:
    ShadingModel:
        - Lit (PBR)
        - Unlit
        - Subsurface (optional later)

    BlendMode:
        - Opaque
        - Masked
        - Transparent

    Parameters:
        - Textures
        - Constants

MaterialInstance:
    overrides parameters only
```

---

# 🔹 4. Shader Variants (VERY IMPORTANT)

Each material generates:

```text id="shader_variants"
Variants:
    - GBuffer (Deferred)
    - Forward+ (lighting inside)
    - Shadow
    - DepthOnly
```

Optional later:

```text id="extra_variants"
    - MotionVectors
    - Velocity
```

---

# 🔹 5. Shader Composition (how to write them)

```glsl id="common"
#include "material_common.glsl"
#include "pbr_lighting.glsl"
```

---

### GBuffer Variant:

```glsl id="gbuffer"
MaterialData mat = GetMaterial();
WriteToGBuffer(mat);
```

---

### Deferred Lighting (GLOBAL shader):

```glsl id="deferred_lighting"
mat = ReadFromGBuffer();
color = ComputePBR(mat, lights);
```

---

### Forward+ Variant:

```glsl id="forward_plus"
mat = GetMaterial();
lights = GetClusterLights();
color = ComputePBR(mat, lights);
```

---

# 🔹 6. Lighting Model Split

```text id="lighting_split"
Deferred:
    Material → writes data
    Lighting → separate fullscreen pass

Forward+:
    Material → does lighting itself
```

---

# 🔹 7. Pass Execution Flow

```text id="execution"
Renderer:

Shadow Pass:
    use Shadow variant

GBuffer Pass:
    bind GBuffer pipeline
    use GBuffer variant

Deferred Lighting:
    fullscreen shader (no material)

Forward+ Pass:
    run light culling (compute)
    bind Forward+ pipeline
    use Forward+ variant

Post:
    bloom, tonemap, etc.
```

---

# 🔹 8. Descriptor Layout (your system — correct)

```text id="descriptor"
Set 0 → Frame (camera, time)
Set 1 → Pass (lights, gbuffer, etc.)
Set 2 → Material
Set 3 → Bindless
Set 4 → Scene (instances, draw data)
```

---

# 🔹 9. Minimal Material Types you NEED

```text id="minimal_types"
ShadingModel:
    - Lit (PBR)
    - Unlit

BlendMode:
    - Opaque
    - Masked
    - Transparent
```

👉 This alone = 90% of AAA

---

# 🔹 10. Final Mental Model

```text id="final_model"
Renderer controls passes
Materials adapt via shader variants
Objects are routed based on material
```

---

# 🔥 Final One-Line Truth

> Unreal-level engine = **Deferred (opaque) + Forward+ (transparent) + material-driven shader variants**

---
