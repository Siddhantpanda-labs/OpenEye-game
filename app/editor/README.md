# OpenEye Editor

Minimal editor layer: client of `api/` only; all world changes go through the mutation layer.

## Build

Enable the editor in CMake, then build:

```powershell
cd build
cmake .. -DOPENEYE_BUILD_EDITOR=ON
cmake --build . --config Debug --target OpenEyeEditor
```

Executable: `build/bin/Debug/OpenEyeEditor.exe`

## Structure

- **EditorApp** — Bootstrap (engine, window, ImGui), main loop, dockspace; owns `EditorContext`; calls `ShowMenuBar(ctx)` and each panel’s `Show()`.
- **EditorContext** — Holds `api::world::World` and current scene path; all edits go through the mutation layer.
- **MenuBar** — File → Open, Save, Save As; path modals (no native file dialog yet).
- **panels/** — One file per dockable panel. All panels use default ImGui flags so they are dockable.
  - `StatusPanel` — Scene path and entity count.
  - `HierarchyPanel` — Entity tree, selection, Create Entity / Create Child, Delete, Rename (Phase 2).
  - `InspectorPanel` — Lists components for selected entity; typed edit for Name and Transform; generic JSON + Apply for others (Phase 3).
  - `SceneViewPanel` — Viewport (Phase 4 placeholder).

## Phase 3 (current)

- Inspector: GetComponentNames → list; Name (InputText), Transform (position/rotation/scale), others as raw JSON with Apply. All via GetComponentJson/SetComponentJson.

## Plan

See `documentation/plans/Editor_Layer_Plan.md`.
