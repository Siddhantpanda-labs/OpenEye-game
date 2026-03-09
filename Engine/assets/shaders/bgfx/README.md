# BGFX shaders

BGFX uses **precompiled shader binaries** (`.bin`). All sources here are in BGFX format (`$input`/`$output`, `SAMPLER2D`, `varying.def.sc`). Compile with the **shaderc** tool and place the `.bin` files where the runtime looks (see below).

## Layout

- **Root** – Blit (copy to backbuffer)
  - `varying.def.sc`, `copy.vert`, `copy.frag`
- **post/** – Post process (fullscreen)
  - `varying.def.sc`, `tonemap`, `bloom_extract`, `bloom_blur`, `bloom_composite` (each `.vert` + `.frag`)
- **forward/** – Forward lit pass (mesh)
  - `varying.def.sc`, `lit.vert`, `lit.frag`

## Building

**Option A – Python script (recommended)**  
From the repo root, run:

```bash
python scripts/compile_bgfx_shaders.py
```

This discovers all `.vert`/`.frag` pairs under `assets/shaders/bgfx/`, compiles them with shaderc, and writes `.bin` files to `build/bin/Debug/assets/shaders/bgfx/<renderer>/`. Default renderer is **vulkan**; use `--renderer gl`, `--renderer d3d11`, or `--renderer all` for others. Run with `--dry-run` to print commands only. Requires `shaderc` on PATH or in the build tree (build BGFX with tools enabled).

**Option B – Manual shaderc**

1. **Build shaderc** (optional): Enable `BGFX_BUILD_TOOLS` and `BGFX_BUILD_TOOLS_SHADER` in the bgfx.cmake submodule, then build. Or use a prebuilt shaderc.

2. **Include path**: `-i <path-to-bgfx>/bgfx/src` (so `#include <bgfx_shader.sh>` resolves). Example: `thirdparty/bgfx.cmake/bgfx/src`.

3. **Compile** from the repo root (or from `assets/shaders/bgfx`). Use the correct `--varyingdef` for each shader.

**OpenGL (e.g. Windows, profile 140):**

```bash
BGFX_SRC=thirdparty/bgfx.cmake/bgfx/src
OUT=build/shaders/bgfx/gl

# Blit (run from assets/shaders/bgfx)
shaderc -f copy.vert -o $OUT/copy_vs.bin --type v --platform windows -p 140 -i $BGFX_SRC --varyingdef varying.def.sc
shaderc -f copy.frag -o $OUT/copy_fs.bin --type f --platform windows -p 140 -i $BGFX_SRC --varyingdef varying.def.sc

# Post (run from assets/shaders/bgfx)
shaderc -f post/tonemap.vert -o $OUT/tonemap_vs.bin --type v --platform windows -p 140 -i $BGFX_SRC --varyingdef post/varying.def.sc
shaderc -f post/tonemap.frag -o $OUT/tonemap_fs.bin --type f --platform windows -p 140 -i $BGFX_SRC --varyingdef post/varying.def.sc
shaderc -f post/bloom_extract.vert -o $OUT/bloom_extract_vs.bin --type v --platform windows -p 140 -i $BGFX_SRC --varyingdef post/varying.def.sc
shaderc -f post/bloom_extract.frag -o $OUT/bloom_extract_fs.bin --type f --platform windows -p 140 -i $BGFX_SRC --varyingdef post/varying.def.sc
shaderc -f post/bloom_blur.vert -o $OUT/bloom_blur_vs.bin --type v --platform windows -p 140 -i $BGFX_SRC --varyingdef post/varying.def.sc
shaderc -f post/bloom_blur.frag -o $OUT/bloom_blur_fs.bin --type f --platform windows -p 140 -i $BGFX_SRC --varyingdef post/varying.def.sc
shaderc -f post/bloom_composite.vert -o $OUT/bloom_composite_vs.bin --type v --platform windows -p 140 -i $BGFX_SRC --varyingdef post/varying.def.sc
shaderc -f post/bloom_composite.frag -o $OUT/bloom_composite_fs.bin --type f --platform windows -p 140 -i $BGFX_SRC --varyingdef post/varying.def.sc

# Forward (run from assets/shaders/bgfx)
shaderc -f forward/lit.vert -o $OUT/lit_vs.bin --type v --platform windows -p 140 -i $BGFX_SRC --varyingdef forward/varying.def.sc
shaderc -f forward/lit.frag -o $OUT/lit_fs.bin --type f --platform windows -p 140 -i $BGFX_SRC --varyingdef forward/varying.def.sc
```

For **Vulkan** use `-p spirv`, for **D3D11** use `-p s_5_0`. Change `$OUT` to `build/shaders/bgfx/vulkan` or `d3d11` as needed.

4. **Where the runtime looks** (in order):
   - `<executable-directory>/shaders/bgfx/<renderer>/`
   - `shaders/bgfx/<renderer>/`
   - `assets/shaders/bgfx/<renderer>/`

   `<renderer>` is `gl`, `d3d11`, or `vulkan`. Copy the compiled `gl/` (or `vulkan/`, `d3d11/`) folder with all `.bin` files into one of these.

## Uniform packing (BGFX post / forward)

- **tonemap**: `u_TonemapParams` = (exposure, useACES, 0, 0)
- **bloom_extract**: `u_BloomExtractParams` = (threshold, 0, 0, 0)
- **bloom_blur**: `u_BlurParams` = (resolution.x, resolution.y, horizontal, intensity)
- **bloom_composite**: `u_BloomCompositeParams` = (bloomIntensity, 0, 0, 0)
- **forward lit**: Material and lights use the uniform names in `lit.frag` (e.g. `u_Albedo`, `u_MaterialParams`, `u_ShadowParams`, point/spot arrays). The render backend must set these when using the BGFX pipeline.
