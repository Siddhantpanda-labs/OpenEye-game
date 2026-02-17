# Test: Forward + Skinned (OpenGL)

Minimal in-app test that runs the **same forward path and flow** as the full player, including **threading**:
- **OpenGL backend only** (no Diligent).
- **Forward pipeline** (set inside `WorldRenderBridge::Initialize`).
- **Same forward shaders** (`passes/forward/skinned` for the model).
- **Hardcoded scene** (no JSON): one camera, one directional light, one skinned mesh (`models/animation/Running.fbx`).
- **Same threading as player**: dedicated render thread + `RenderFrameSync` (double-buffered slots, AcquireFrame / GetFillSlot / SubmitFrame / ReleaseFrame). Use this to repro flicker that appears only with the threaded path.

No overlay, physics, or scripting.

## Build

Built when `OPENEYE_BUILD_PLAYER` is ON:

```bash
cmake --build build --target TestForwardSkinned
```

Output: `build/bin/Debug/TestForwardSkinned.exe` (or `Release`).

## Run

Run from the **project root** so `assets/` is found (no scene JSON; scene is hardcoded):

```bash
# From repo root
./build/bin/Debug/TestForwardSkinned.exe
```

Or from `build/bin/Debug` after copying (or symlinking) `assets/` there so `models/animation/Running.fbx` and shaders resolve.

Log file: `TestForwardSkinned.log` (next to the executable or cwd).
