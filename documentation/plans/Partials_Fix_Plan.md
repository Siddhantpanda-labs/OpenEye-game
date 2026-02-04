# Partials Fix Plan — Clean, One-by-One

This document lists each **partial** from the architecture checklist and the **concrete plan** to fix it properly. No mess; one pillar at a time.

---

## 1. World Mutation System (Write Authority) ✅ (Done)

**Current gap (addressed):**  
- Commands, transactions, undo/redo exist.  
- **Scene load bypasses them:** `World::LoadScene(path)` calls `Serializer::Load(state, path)` and mutates state directly.  
- So there are two ways to change structure: (1) commands, (2) direct load.

**Target:**  
- **One legal way to change structure:** all structural changes (including “load scene”) go through the mutation system (commands + apply).  
- Validation and undo/redo apply to scene load as well.

**Plan:**

1. **Introduce a “LoadScene” command**  
   - New command type (e.g. `LoadSceneCommand`) that carries scene path (or buffer/source id).  
   - Validator: can allow it only at “boundary” (e.g. no undo of full load, or special rule).  
   - Executor: calls existing `WorldSerializer::Load(state, path)` (or `Deserialize(state, data)` when you add buffer API). So implementation reuse, no duplicate logic.

2. **Make `World::LoadScene(path)` go through command**  
   - `World::LoadScene(path)` becomes: enqueue `LoadSceneCommand(path)`, then `ApplyPendingCommands()` (or a single “load and apply” that does both so callers don’t change).  
   - Remove any path that lets external code call `Serializer::Load(state, path)` or mutate state for “scene load” without going through World’s command API.

3. **Optional but recommended**  
   - One “ReplaceWorld” or “LoadScene” transaction that wraps the command so undo/redo can “restore previous world” if you want (e.g. snapshot before load, undo restores snapshot).  
   - If you don’t want undo for full load, document it and ensure validator doesn’t require it.

**Result:** Scene loading is a first-class mutation; no hidden back door. Same pattern can later be used for “load additive” or “unload region”.

**Done:** LoadSceneCommand added (CommandTypes.h); ApplyCommandSync added to eyelib::world::World; CommandValidator validates LoadSceneCommand (path non-empty); api::World::LoadScene(path) now calls ApplyCommandSync(LoadSceneCommand(path)) instead of Serializer::Load directly.

---

## 2. Observation Layer (Read Authority) ✅ (Done)

**Current gap (addressed):**  
- `WorldObservation` + `WorldView` exist and are read-only.  
- **World API still exposes direct getters:** `GetTransforms()`, `GetRenderables()`, `GetCameras()`, `GetLights()`, etc., which read straight from `GetState().Components<...>().All()`.  
- So code (player, Lua, etc.) can read world without going through the observation layer; “nothing touches raw memory” is not enforced.

**Target:**  
- All read access to world state goes through a single read facade (Observation / View).  
- No direct `GetState().Components<>()` from outside the engine core.

**Plan:**

1. **Extend Observation (or WorldView) to cover current use cases**  
   - Add to `api::world::Observation` (or the read facade you choose) the queries that the player and Lua need: e.g. `GetTransforms()`, `GetRenderables()`, `GetCameras()`, `GetLights()`, `GetMeshRenderers()`, and any other batch getters currently on `World`.  
   - Implementation: Observation holds a `WorldView` (const state); these methods read from `m_view.State().Components<...>()` and return the same structs (e.g. `World::TransformEntry`) or equivalent. So one place reads from state; that place is the observation layer.

2. **Route all external read through Observation**  
   - **World:** Keep `GetView()` / `GetObservation()` (or add `const Observation& GetObservation() const` that returns a view-backed Observation).  
   - **Deprecate or remove** direct batch getters from `World` (e.g. `GetTransforms()`, `GetRenderables()`, …).  
   - **Player and Lua:** Use `world.GetObservation()` (or `Observation obs(world)`), then `obs.GetTransforms()`, `obs.GetRenderables()`, etc.  
   - Internal engine code (e.g. systems that run inside `World::Update()`) may still get `WorldView` or `const WorldState&` from the world for performance; the rule is “no raw state from outside.”

3. **Document the rule**  
   - “External read-only access to world is only via Observation (or WorldView). World’s mutable API is commands; read API is Observation.”

**Result:** Single read path; no direct state access from outside; observation layer is the only way to “see” the world from editor, AI, tools, player.

**Done:** WorldObservation::GetView() added; Observation has GetTransforms, GetRenderables, GetMeshRenderers, GetCameras, GetLights, GetRigidBodies, GetColliders; World::GetObservation() added; player (WorldRender, WorldPhysics) and Lua use GetObservation() for batch reads; World batch getters deprecated with @deprecated comments.

---

## 3. Runtime Systems — Audio ✅ (Done)

**Current gap (addressed):**  
- Physics, animation, rendering, scripting are wired in the player.  
- Audio exists in the engine but is **not** wired in the runtime player.

**Target:**  
- Audio is a first-class runtime system in the player: world can define audio sources/listeners; player updates them each frame (no commands, no undo; read component data, drive audio API).

**Plan:**

1. **Define world components for audio** (if not already)  
   - e.g. `AudioSource`, `AudioListener` (or reuse/extend existing).  
   - Scene JSON can author them.

2. **Add an audio bridge in the player**  
   - `WorldAudioBridge`, analogous to `WorldPhysicsBridge` / `WorldAnimationBridge`:  
     - `BuildFromWorld(world)` — collect audio entities from world (via Observation when that’s the read path).  
     - `Update(delta)` — push positions/volumes to the audio API.  
   - No commands; read-only read of transforms/sources, then drive OpenAL (or whatever you use).

3. **Wire in the run loop**  
   - After animation (or in a fixed order), call `audioBridge.Update(world, delta)` (or pass Observation).  
   - Init/shutdown next to other bridges.

**Result:** Audio is part of “runtime systems (execution muscle)” and behaves like physics/animation: direct read, no commands, no undo.

**Done:** WorldAudioBridge added (WorldAudio.h/cpp); Initialize/Shutdown, BuildFromWorld (no-op for now), Update(world, delta) syncs listener from primary camera via Observation; PlayerApp creates bridge, initializes when !headless, calls BuildFromWorld and Update in loop, Shutdown on exit. Listener position/orientation driven from primary camera transform.

---

## 4. Asset System — Async Loading & Hot Reload

**Current gap:**  
- Asset IDs, registry, dependency tracking, reload exist.  
- Loaders have **async** entry points (`LoadMeshAsync`, `LoadTextureAsync`, etc.), but the **player’s asset binding is synchronous** (load everything at startup).  
   - No async loading in the player flow.  
   - Hot reload: only shaders are watched; materials/meshes/animations are not.

**Target:**  
- World references assets symbolically (you already do).  
- Optional async loading so large worlds don’t block on load.  
- Optional hot reload for assets (e.g. materials, textures) in dev.

**Plan:**

1. **Async loading (minimal, clean)**  
   - **Option A (lightweight):** Keep current “load at startup” as default. Add an **optional** path: e.g. `BindAssetsAsync(world, roots, registry, callback)` that:  
     - Collects refs (as now).  
     - For each asset, calls existing async loaders; when all are done, invokes callback (e.g. “ready to run”).  
     - Player can choose sync (current) or async (for large scenes).  
   - **Option B (deeper):** Registry supports “pending” IDs; systems (e.g. renderer) tolerate missing assets and show placeholder or hide until loaded.  
   - Start with Option A so you don’t block; Option B can come later if you need streaming.

2. **Hot reload (targeted)**  
   - Extend the “watcher” idea (you have ShaderWatcher) to a small **AssetWatcher** (or reuse name):  
     - Watch material/texture/mesh files under asset roots (or a whitelist).  
     - On change: call `registry.ReloadAsset(id)` (or equivalent) and invalidate caches that use that asset (e.g. renderer’s material cache).  
   - Keep it dev-only and optional so shipped builds have zero cost.

3. **Dependency graph**  
   - You already have `GetDependencies` / `GetDependents`. Ensure that when an asset is reloaded, dependents are refreshed or marked dirty so “one serializer, one source of truth” isn’t broken (e.g. material reload updates meshes that reference it).

**Result:** Professional asset system: optional async load, optional hot reload, world references assets by ID; no raw file IO from game code.

---

## 5. Serialization — One Truth, One Path ✅ (Deprecation done)

**Current gap (addressed):**  
- **World state** is serialized by `WorldSerializer` (JSON) and used for scene load and (conceptually) save/checkpoints.  
- There is a **separate** path: `scene::Scene` + `BinarySerializer::LoadFromFile(scene::Scene, path)` and `SceneManager`.  
- So you have two “scene” concepts: (1) WorldState + WorldSerializer, (2) Scene + BinarySerializer.  
- The **runtime player only uses (1)**; (2) is unused by the player but exists in the codebase.

**Target:**  
- **One serializer for “world state”** (scenes, save games, checkpoints, AI snapshots).  
- One data model: WorldState. One serialization path for that model (e.g. WorldSerializer).  
- No parallel “scene” format that duplicates or replaces world state.

**Plan:**

1. **Declare WorldState + WorldSerializer as the single source of truth**  
   - Scenes = serialized WorldState (current JSON).  
   - Save games / checkpoints / AI snapshots = same: serialize WorldState (and optionally strip or add components).  
   - Document: “All persistent world state is WorldState; all load/save goes through WorldSerializer (or a future binary variant that still targets WorldState).”

2. **Unify or remove the duplicate path**  
   - **Option A (recommended):** Deprecate/remove `scene::Scene` and `BinarySerializer` for scene loading.  
     - If something (e.g. an old tool) still needs binary, add a **WorldSerializer::SaveToBinary** / **LoadFromBinary** that still reads/writes **WorldState**, not a different Scene type.  
   - **Option B:** Keep the binary path only if it’s explicitly “legacy” or “editor-only” and document that the **runtime and all new features** use WorldState + WorldSerializer only.

3. **Single entry point for “load world”**  
   - Runtime and tools: “load scene” = load into WorldState via Serializer (path or buffer).  
   - No second loader that fills a different structure.

**Result:** One serializer, one model (WorldState); scenes and save games are the same thing; no architectural split.

**Done:** `documentation/Serialization_Authority.md` added; SceneManager and BinarySerializer(Scene) deprecated in headers with pointer to that doc. Runtime player already uses World::LoadScene(path) only.

---

## Order of Work (Suggested)

1. **Serialization (5)** — Unify or remove the second path so there’s one truth. Small, clear change.  
2. **Observation (2)** — Add read facade and route all external read through it; then mutation and systems can rely on “read only via Observation.”  
3. **Mutation (1)** — Scene load via command; no direct Serializer::Load from outside.  
4. **Audio (3)** — Wire audio bridge so “runtime systems” is complete.  
5. **Assets (4)** — Async + hot reload as optional, incremental improvements.

This order avoids redoing work: serialization is cleaned first; then read path; then write path; then systems and assets.

---

## Summary Table

| Partial              | Target                                      | Key change                                                                 |
|----------------------|---------------------------------------------|----------------------------------------------------------------------------|
| Mutation system      | One legal way to change structure           | LoadScene via command; no direct Serializer::Load from World API.         |
| Observation layer    | All read through one facade                  | Observation has all getters; World direct getters removed/deprecated.      |
| Runtime systems      | Audio wired in player                       | WorldAudioBridge, Update in loop.                                          |
| Asset system         | Async + hot reload (optional)               | BindAssetsAsync; AssetWatcher + ReloadAsset.                               |
| Serialization        | One serializer, one model (WorldState)      | WorldSerializer only; deprecate/remove Scene+BinarySerializer for scenes. |
