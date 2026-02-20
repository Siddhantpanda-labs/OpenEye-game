# Editor Layer Plan

Plan for implementing the **Editor Layer** as a **minimal** client application on top of the OpenEye API. The editor is the primary way to exercise and validate each feature and module of the library.

---

## Scope: minimal first

- **Minimal** means: the smallest set of features needed to **make a simple game and play it** (load scene, edit entities/components, save, run in player or play in editor), and to **test each library module** through the API.
- No Unity-level tooling: no full asset pipeline, no prefab system, no complex animation/terrain/lighting editors. Add those only if needed later (see `Editor_GUI_Professional.md` for a long-term vision).
- Phases 0–4 are the **minimal viable editor** (bootstrap, load/save, hierarchy, inspector, scene view). Later phases add one area at a time (assets, physics, animation, audio, scripting, play mode) so the editor stays minimal but still exercises the full API.

---

## 1. Principles

### 1.1 Client-side only

- The editor is a **client** of the engine: it lives in `app/editor/` and **only** uses the public **`api/`** surface.
- It must **never** include `eyelib/` headers directly. All engine access goes through `api/core`, `api/world`, `api/render`, etc.
- This keeps the library UI-free and guarantees the editor can be rebuilt against API changes without depending on implementation details.

### 1.2 All changes through the mutation layer

- Every change to the world (create/delete entity, reparent, add/remove component, edit component data) goes through the **World Mutation System**.
- The editor produces **commands** (e.g. via `api::world::World::SubmitCommand`, `ApplyCommandSync`, or high-level helpers that wrap them). The library applies commands; the editor never mutates world state directly.
- Save to disk is serialization of the same world state (e.g. `WorldSerializer::Save` or equivalent API). Load is deserialization into the world via the mutation layer (e.g. `LoadScene` command).
- This aligns editor behavior with AI, gameplay, and networking: one writer, one command pipeline, deterministic and serializable.

### 1.3 Editor as testbed for library modules

- The editor is designed to **use and test every major API module**. Each feature area (panels, tools, play mode) should exercise specific modules so that the editor effectively validates the library in integration.
- Implementation order can follow “minimal viable editor first,” then add panels/features that pull in additional modules (e.g. audio, scripting, particles) and document which module each part tests.

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Editor App (app/editor/)                                    │
│  - ImGui docking, panels, viewport, menus                    │
│  - Only includes api/*                                      │
└───────────────────────────┬─────────────────────────────────┘
                            │ uses
┌───────────────────────────┴─────────────────────────────────┐
│  Public API (api/)                                           │
│  - world, render, physics, animation, audio, scripting,       │
│    scene, assets, platform, debug, geometry, ...             │
└───────────────────────────┬─────────────────────────────────┘
                            │ implemented by
┌───────────────────────────┴─────────────────────────────────┐
│  Engine library (eyelib/)                                    │
│  - World mutation, serialization, render, physics, etc.      │
└─────────────────────────────────────────────────────────────┘
```

- **Mutation path:** Editor UI → commands (e.g. CreateEntity, SetComponentData) → `api::world::World` → internal command buffer → world state updated. No direct writes to world state from the editor.

---

## 3. Module coverage (editor testing which API modules)

The editor should eventually exercise each library area as follows. Phases can be ordered so that “core editor” comes first and additional modules are tested as panels/tools are added.

| API module      | How the editor tests it |
|-----------------|--------------------------|
| **api/core**    | Engine init, window, logging; editor lifecycle. |
| **api/platform**| File dialogs, paths, asset roots, optional shader/file watching. |
| **api/world**   | Load/Save scene, create/delete entity, hierarchy, tags, metadata, Get/SetComponentJson, transactions (undo/redo), events. |
| **api/scene**   | Scene references if used; scene list or multi-scene if exposed. |
| **api/serialize** | If exposed for custom formats; otherwise world serialization via world API. |
| **api/assets**  | Asset registry, load mesh/material/texture, resolve paths; asset browser. |
| **api/math**    | Vectors, matrices, transforms used in Inspector and gizmos. |
| **api/render**  | Viewport rendering, editor camera, meshes, materials, lights, debug draw. |
| **api/physics** | Colliders, rigid bodies in Inspector; optional physics preview in viewport. |
| **api/animation** | Animator component, clip playback; optional animation preview. |
| **api/audio**   | Audio sources/listener in world; optional audio preview or level meters. |
| **api/scripting** | Script components, property edit; optional script reload. |
| **api/geometry** | Picking, raycast, selection in viewport (if exposed). |
| **api/debug**   | Debug draw, stats, diagnostics overlay. |
| **api/ecs**     | If exposed for advanced queries; otherwise world entity/component API suffices. |
| **api/particles** | If editor supports particle entities/components. |
| **api/fluids**  | If editor supports fluid entities/components. |

Early phases focus on **core, platform, world, assets, math, render** so the editor can load a scene, show hierarchy, inspect/edit components, and render the scene view. Later phases add **physics, animation, audio, scripting, geometry, debug** and optionally particles/fluids.

---

## 4. Phased implementation

**Minimal viable editor:** Phases 0–4 (bootstrap through scene view). Phases 5–9 extend coverage without bloating the initial scope.

### Phase 0: Bootstrap

- Add `app/editor/` and CMake target; link against required API libraries (core, platform, world, assets, math, render, ImGui/ImGuizmo).
- Editor window + ImGui docking; no panels yet.
- **Tests:** core (init/shutdown), platform (window).

### Phase 1: Load/Save and world in memory ✅

- Load scene from JSON (path from dialog or command line) via world API (e.g. `LoadScene`); save current world to JSON via serialization API.
- Menu: File → Open, Save, Save As.
- **Tests:** world (load/save), serialize (scene format).
- **Done:** `World::SaveScene` in API; `EditorContext` (World + scene path); `MenuBar` with path modals; Status panel shows scene path and entity count.

### Phase 2: Hierarchy and selection ✅

- Hierarchy panel: list entities and parent-child structure using world API (GetEntities, GetChildren, GetParent); selection state in editor.
- Create/delete entity, rename, reparent via world commands.
- **Tests:** world (entities, hierarchy, metadata, commands).
- **Done:** `EditorContext.selectedEntity`; Hierarchy panel tree (roots + children), selection on click, Create Entity, context menu (Create Child, Rename, Delete); Inspector shows selected entity.

### Phase 3: Inspector and component edit ✅

- Inspector panel: for selected entity, list components (GetComponentNames), show/edit via GetComponentJson/SetComponentJson (or typed helpers if available).
- At least Transform, Name; then MeshRenderer, Camera, Light, Collider, RigidBody, etc., as needed.
- **Tests:** world (component read/write), math (transform display).
- **Done:** Inspector lists all components; Name (InputText + SetComponentJson/SetEntityName), Transform (position/rotation/scale InputFloat3/4 + SetComponentJson); generic components: raw JSON multiline + Apply.

### Phase 4: Scene view (viewport)

- Scene view panel: render the current world from an **editor camera** (separate from game camera) into a framebuffer; display in ImGui.
- Editor camera controls (orbit/pan); optional gizmo (translate/rotate/scale) using ImGuizmo.
- **Tests:** render (pipeline, camera, meshes, materials, lights).

**Phase 4a (done):** Viewport panel with content region, placeholder background, and `EditorCamera` in context (distance, yaw, pitch, focus, fov). Ready for wiring render path. Player remains independent.

**Phase 4b (done):** Render world to texture: GetOutputColorTextureNativeHandle(); EditorSceneBuilder (snapshot → RenderScene). Pipeline + AssetRegistry init; each frame: build scene, resize pipeline, Render(scene), display texture. No shared builder; editor-only “BuildRenderSceneFromWorld” using API only, then scene view renders world from `editorCamera` to a `RenderTarget`.

### Phase 5: Asset and resource integration

- Asset roots configuration; resolve mesh/material references from world to assets.
- Optional asset browser panel: list assets, assign to components (e.g. mesh, material).
- **Tests:** assets (registry, resolution, loading).

### Phase 6: Physics and animation in editor

- Inspector support for physics (collider, rigid body) and animation (animator, clips); optional “preview” in viewport (e.g. single-step physics or play animation).
- **Tests:** physics, animation.

### Phase 7: Audio and scripting

- Inspector for audio components; optional audio preview.
- Inspector for script components (properties); optional script reload.
- **Tests:** audio, scripting.

### Phase 8: Play mode and debug

- “Play” in editor: run game loop (update world, render, physics, etc.) on the same in-memory world; “Stop” returns to edit state.
- Optional: debug overlay (stats, diagnostics) using debug API; geometry (picking) if available.
- **Tests:** full integration of world, render, physics, animation, scripting; debug.

### Phase 9: Optional modules

- Particles, fluids, geometry (advanced picking), or other modules as needed; add panels or tools that use them and document which module is tested.

---

## 5. File and target layout

- **`app/editor/`**  
  - Editor executable target; only links to `api/*` and shared deps (ImGui, GLFW, etc.).
- **`app/editor/CMakeLists.txt`**  
  - Lists sources and links to EyelibAPI libraries (no direct eyelib target).
- **Sources (suggested):**  
  - `main.cpp` — entry, window, loop.  
  - `EditorApp.cpp/.h` — init, shutdown, frame (update world, render viewport, ImGui).  
  - Panels: `HierarchyPanel`, `InspectorPanel`, `SceneViewPanel`, optional `AssetBrowserPanel`, etc.  
  - Optional: `EditorWorldBridge` or similar that only calls `api::world::World` and serialization for load/save.

No editor-specific code inside `eyelib/` or `api/` beyond what is already there; the editor is purely a client in `app/editor/`.

---

## 6. Success criteria

- Editor builds and runs using only `api/` (no `eyelib/` includes in `app/editor/`).
- All world edits go through the world API (commands / mutation layer); no direct manipulation of engine internals.
- Each major API module is exercised by at least one editor feature (documented in “Module coverage” above).
- User can open a scene file, edit entities and components, save, and run the same scene in the player (or play in editor) with consistent behavior.

---

## 7. Related documents

- `Editor_GUI_Professional.md` — long-term editor ecosystem and structure.
- `world_mutation_system_implementation_plan.md` — command types, serializer, transactions.
- `EditorAndAiplan.md` — editor vs AI as command producers; scene file as snapshot.
