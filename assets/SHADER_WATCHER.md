# Shader file watcher

When the app runs from a **runtime folder** (e.g. `build/bin`) but you edit shaders in the **root project** `assets/shaders`, the shader watcher keeps them in sync.

- **Source**: root repo `assets/shaders` (where you edit).
- **Runtime**: `assets/shaders` next to the running executable (where the app loads from).

On each poll (~800 ms), any shader file (`.vert`, `.frag`, `.glsl`, `.vsh`, `.psh`, `.comp`, `.geom`, `.tesc`, `.tese`) that is newer in the source directory is copied into the runtime directory with the same relative path. No restart needed for file changes to be present at runtime (reload of shaders by the app is separate).

The **runtime player** (`OpenEyePlayer`) and the test app (`OpenEyeApiTests`) enable the watcher automatically:

- If the run directory has `../../assets/shaders` (e.g. you run from `build/bin`), that is used as **source** and `assets` next to the exe as **runtime**.
- Otherwise source and runtime both use `assets` relative to the current working directory.

To have runtime shaders updated when running from `build/bin`, either:

1. Run the app with working directory = project root, or  
2. Copy or symlink the project `assets` folder into `build/bin` so `build/bin/assets` exists; the watcher will then copy changed shaders from the repo `assets/shaders` into `build/bin/assets/shaders`.

**Forward mode:** For point/spot lights in Forward mode, use materials with shader `../shaders/passes/forward/lit` (e.g. in your `.mat` file set `"shader": "../shaders/passes/forward/lit"`).
