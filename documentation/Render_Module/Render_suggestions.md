Good. Same discipline, different layer. Let me think about this properly — rendering references have a different landscape than RHI references.

---

### The problem with rendering references

Unlike RHI, where Diligent + Wicked + Khronos samples covers almost everything, **no single repo covers all of rendering well**. Different engines are best-in-class for different techniques. You need a curated set matched to each domain.

---

### By technique — best references

**Deferred + Forward / Base renderer structure**
```
SaschaWillems/Vulkan
```
This is your primary for renderer structure. Sascha's repo has isolated, single-file samples for almost every rendering technique — deferred shading, forward+, subpasses, MSAA, bloom, SSAO — each one clean and focused. Unlike full engines, there's no framework noise. Direct Vulkan + technique.

Key files:
```
examples/deferred/deferred.cpp
examples/deferredshadows/deferredshadows.cpp
examples/ssao/ssao.cpp
examples/bloom/bloom.cpp
examples/multisampling/multisampling.cpp
examples/hdr/hdr.cpp
```
https://github.com/SaschaWillems/Vulkan

---

**PBR — physically based rendering**

Two references, used together:

Primary — **Sascha's PBR samples** (Vulkan implementation):
```
examples/pbr/pbr.cpp                  ← base PBR
examples/pbrbasic/pbrbasic.cpp        ← simpler entry point
examples/pbribl/pbribl.cpp            ← IBL specular + diffuse
examples/pbrtexture/pbrtexture.cpp    ← textured PBR materials
```

Secondary — **Filament's shading model docs + source** (algorithm reference, not Vulkan):
```
filament/src/shaders/brdf.fs          ← BRDF implementation
filament/src/shaders/shading_model_standard.fs
filament/src/shaders/ambient_occlusion.fs
```
Filament's shading is the quality bar. Sascha shows you the Vulkan wiring. Use both.

---

**Shadows**

```
SaschaWillems/Vulkan:
examples/shadowmapping/shadowmapping.cpp       ← basic PCF
examples/shadowmappingcascade/shadowmappingcascade.cpp  ← CSM
examples/shadowmappingomni/shadowmappingomni.cpp        ← point light / cube shadow
```

For PCSS and more advanced shadow filtering, **Wicked Engine** is better:
```
WickedEngine/shaders/shadowPS.hlsl
WickedEngine/shaders/shadowVS.hlsl
```
Wicked has soft shadows, cascade blending, and shadow bias handling that Sascha's samples don't cover.

---

**Global Illumination**

This is the hardest area — GI has no single clean reference. Split by technique:

*SSAO / GTAO:*
```
SaschaWillems/Vulkan:
examples/ssao/ssao.cpp

Wicked Engine:
WickedEngine/shaders/ssaoCS.hlsl       ← GTAO implementation
```

*Voxel GI / VXGI:*
```
Wicked Engine:
WickedEngine/shaders/voxelRadianceCS.hlsl
WickedEngine/shaders/voxelConeTracingCS.hlsl
```
Wicked has one of the better open-source VXGI implementations.

*Screen-space GI / reflections:*
```
Wicked Engine:
WickedEngine/shaders/ssgiCS.hlsl
WickedEngine/shaders/SSR_*.hlsl
```

*Lumen-style dynamic GI — if you want to go that far:*
```
https://github.com/EthanCS/LumenPBR
```
Small research repo implementing Lumen-inspired GI in Vulkan. Useful as a conceptual reference, not production quality.

---

**Ray Tracing**

Two references:

Primary — **Sascha's RT samples:**
```
examples/raytracingbasic/raytracingbasic.cpp
examples/raytracingshadows/raytracingshadows.cpp
examples/raytracingsoftoshadows/raytracingsoftshadows.cpp
examples/raytracingrefraction/raytracingrefraction.cpp
examples/raytracingreflections/raytracingreflections.cpp
```
Clean isolated RT samples showing BLAS/TLAS setup, SBT, ray gen/miss/hit shaders.

Secondary — **Wicked Engine RT shaders:**
```
WickedEngine/shaders/rtaoCS.hlsl           ← RT ambient occlusion
WickedEngine/shaders/rtshadowCS.hlsl       ← RT shadows
WickedEngine/shaders/rtreflectionCS.hlsl   ← RT reflections
```
Wicked shows how RT integrates into a full deferred renderer, not just standalone.

---

**Lighting — area lights, IES, clustered**

```
Wicked Engine:
WickedEngine/shaders/lightingHF.hlsli     ← lighting functions header
WickedEngine/shaders/tiledLightingCS.hlsl ← tiled/clustered lighting
WickedEngine/shaders/deferredCS.hlsl      ← deferred lighting pass
```

For clustered lighting specifically, also reference:
```
https://github.com/pezcode/Cluster
```
Small focused clustered forward renderer. Clean implementation of light culling, no engine noise.

---

**Post processing — tonemapping, bloom, DOF, TAA**

```
SaschaWillems/Vulkan:
examples/hdr/hdr.cpp                ← HDR pipeline + tonemapping
examples/bloom/bloom.cpp            ← threshold + blur bloom

Wicked Engine:
WickedEngine/shaders/toneMapPS.hlsl
WickedEngine/shaders/dofCS.hlsl     ← depth of field
WickedEngine/shaders/taaCS.hlsl     ← temporal anti-aliasing
```

---

**Shaders — general GLSL/HLSL reference**

For shader-level implementations of specific algorithms, two resources:

```
https://github.com/google/filament  (GLSL, under filament/src/shaders/)
https://github.com/turanszkij/WickedEngine  (HLSL, under WickedEngine/shaders/)
```

Filament shaders are GLSL and heavily commented with paper references. Wicked shaders are HLSL but show real production usage. Between them you can cross-reference almost any shading algorithm.

---

### Summary map

| Technique | Primary | Secondary |
|---|---|---|
| Deferred / Forward structure | SaschaWillems/Vulkan | Wicked Engine |
| PBR BRDF | SaschaWillems/Vulkan (wiring) | Filament shaders (algorithm) |
| IBL | SaschaWillems `pbribl` | Filament `ibl.fs` |
| Shadows / CSM | SaschaWillems | Wicked Engine shaders |
| SSAO / GTAO | SaschaWillems | Wicked Engine shaders |
| Voxel GI | Wicked Engine | — |
| SSR / SSGI | Wicked Engine | — |
| Ray tracing | SaschaWillems RT samples | Wicked Engine RT shaders |
| Clustered lighting | Wicked Engine | pezcode/Cluster |
| Post processing / TAA | SaschaWillems | Wicked Engine shaders |

---

### One thing to keep in mind

Sascha's samples use **legacy render passes** (pre-dynamic rendering). When you reference his rendering technique samples, the **algorithm and pass structure** is what you extract — not the render pass setup code, which you replace with your dynamic rendering path. The shading logic, attachment layout, and pass ordering are all still valid.