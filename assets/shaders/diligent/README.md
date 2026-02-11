# Diligent shaders (HLSL)

Shaders for the **Diligent** RHI backend. The render module uses **logical shader names** and **paths relative to the shaders folder** only; this folder is where the Diligent backend resolves those names.

## Convention

- **Logical name** (used by the render module): e.g. `passes/forward/lit`, `passes/deferred/lighting`, `passes/post/tonemap`, `debug/lines`. The `passes/` prefix is part of the logical path, not hardcoded by the backend.
- **Diligent resolution**: the backend **always** looks under `assets/shaders/diligent/` (never `assets/shaders/passes/`). Path = `assets/shaders/diligent/` + logical name + `.vsh` / `.psh`.

So for the name `passes/forward/lit`, the backend loads:
- `assets/shaders/diligent/passes/forward/lit.vsh`
- `assets/shaders/diligent/passes/forward/lit.psh`

Exe directory is tried first (e.g. `{exe_dir}/assets/shaders/diligent/...`), then CWD.

## Format

- **Language**: HLSL.
- **Entry point**: `main` for both vertex and pixel shaders.
- **Vertex attributes**: use `ATTRIB0`, `ATTRIB1`, ... in the vertex shader so they match the engine’s input layout (same order as in OpenGL/BGFX passes).
- **Textures**: use `Texture2D` + `SamplerState` with `register(t0)`, `register(s0)`, etc., as required by the pipeline.

## Other backends

- **OpenGL**: resolves names to `shaders/opengl/{name}.vert` and `.frag` (GLSL).
- **BGFX**: resolves to `shaders/bgfx/{renderer}/{baseName}_vs.bin` and `_fs.bin`.

No conflict: each backend only reads from its own subfolder under `shaders/`.
