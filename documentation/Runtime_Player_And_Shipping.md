# Unified Runtime vs Unity-Style Shipping — In Your Setup

This document explains: (1) what a **unified runtime** is, (2) what **Unity-style build/shipping** is, (3) how little they differ, and (4) how both are done in OpenEye with the world module — including the **unified loader** and **Unity-style shipping**.

---

## 1. Two names for the same thing

| Concept | Meaning |
|--------|---------|
| **Unified runtime** | One executable that can run **any** game, as long as you give it the right **scene file + assets**. No per-game recompile. |
| **Unity-style shipping** | You **package** that same executable (the “player”) plus a **specific game’s data** (scene file + assets). The player is generic; the data defines the game. |

So:

- **Unified runtime** = the generic “engine player” (like Unity Player).
- **Unity-style shipping** = how you **build/ship** a title: same player + different data per game.

They are not two different systems. The unified runtime **is** what you ship in Unity-style; the only thing that changes per game is **which scene file and which assets** you bundle (or pass at launch).

---

## 2. What the runtime actually does (the pipeline)

Both “unified runtime” and “Unity-style shipping” assume the same execution pipeline:

```
Executable starts
       ↓
Initialize engine modules (render, physics, audio, etc.)
       ↓
Load assets (or register asset roots for lazy load)
       ↓
Deserialize scene file → WorldState
       ↓
Construct world (entities, hierarchy, components, regions)
       ↓
Start scheduler loop
       ↓
Systems run every frame (physics, render, animation, scripts, etc.)
```

- **Scene file** = initial world state + bindings (which systems, which scripts, which assets). **No custom game logic is compiled in**; logic lives in systems/scripts, and the scene only wires them.
- So: **one runtime, many games** = same pipeline, different scene + assets.

---

## 3. In your setup: what you already have vs what “unified” adds

### Already in place (eyelib / api / world)

- **World state**: `WorldState` (entities, graph, component stores).
- **Scene persistence**: `Serializer::Load(state, path)` / `Save(state, path)` — scene file ↔ `WorldState`.
- **Mutation**: Commands + validation + transactions (undo/redo).
- **Execution**: `WorldScheduler` (fixed timestep, pause, step, time scale, replay).
- **Observation**: `WorldObservation` / `Observation` for read-only queries.
- **Engine modules**: Render, physics, animation, etc. (existing eyelib).

So the **simulation machine** and **scene data model** are already there. What’s missing for a “unified runtime” is **one standard entry path** that always does: init → load assets → load scene into world → run scheduler. Right now the app entry point (e.g. `main.cpp`) is hardcoded to one test (e.g. CharacterTest with fixed paths).

### What “unified loader” means here

**Unified loader** = the single code path that:

1. **Initializes** engine modules (once).
2. **Resolves asset roots** (e.g. command-line, config, or bundled path).
3. **Loads the scene file** into `WorldState` via `Serializer::Load(worldState, scenePath)`.
4. **Optionally** resolves asset references in the scene (meshes, textures, scripts) and loads them.
5. **Starts the scheduler** and runs the loop, with systems reading from `WorldState` and mutating via commands.

So:

- **Unified runtime** = engine + this **unified loader** + scheduler. No game-specific code in the exe; the game is “load this scene + these assets.”
- **Unified loader** = the small layer that wires “scene path (+ asset roots)” → `WorldState` → scheduler. In your codebase that’s: `Serializer::Load` + (optional) asset resolution + `World::Update()` (or equivalent) in a loop.

---

## 4. How “unified runtime” and “Unity-style shipping” are done in your setup

### Option A: One executable, scene path at launch (simplest “unified” runtime)

- **Build**: One player exe (engine + unified loader + all systems).
- **Run**: e.g. `OpenEyePlayer.exe --scene levels/main.json --assets assets/`
- **Flow**: Entry point → init engine → `Serializer::Load(state, scenePath)` → load/reference assets from `--assets` → run scheduler. Different game = different `scenePath` (and different assets on disk).

Same runtime, same loader; only **data** (scene file + asset folder) changes. This **is** your unified runtime.

### Option B: Unity-style “build” per game (shipping a specific title)

- **Build**: Same player exe as in A, but you **package** it with a **default scene** and a **fixed asset set** (e.g. in a `data/` folder or embedded). The exe might still accept `--scene` override, or it might always load a default (e.g. `data/scenes/main.json`).
- **Ship**: You ship one folder/installer: `GameTitle/` containing:
  - `OpenEyePlayer.exe` (or renamed to `GameTitle.exe`),
  - `data/scenes/main.json` (and maybe more scenes),
  - `data/assets/` (meshes, textures, scripts, etc.).

Again: **same runtime, same loader**. The only difference from A is **packaging**: you fix the default scene and bundle the assets so the user doesn’t pass paths. Conceptually it’s still “unified runtime + this game’s data.”

### How different are they?

- **Unified loader**: One code path (init → load scene → load assets → run scheduler). You implement it once.
- **Unified runtime**: That loader + engine + systems in one exe; it can run any game by pointing it at a scene + assets.
- **Unity-style shipping**: Same exe, but for a specific game you **bundle** a default scene + assets and optionally hide the scene path from the user.

So: **one implementation** (unified loader + pipeline) serves both “unified runtime” and “Unity-style build/shipping.” The **difference** is only how you **invoke** and **package** (command-line vs default paths, and what you put in the installer).

---

## 5. What to implement (minimal)

To get both unified runtime and Unity-style shipping in your setup:

1. **Entry point**  
   - Parse or config: scene file path, optional asset root(s).  
   - If no args: use defaults (e.g. `scenes/main.json`, `assets/`) for “shipped” builds.

2. **Init**  
   - Initialize engine (render, physics, etc.) once.

3. **Load scene into world**  
   - Create or get `WorldState` (or `api::world::World` that owns it).  
   - `Serializer::Load(state, scenePath)`.  
   - Apply any needed post-load fixup (e.g. resolve asset paths to loaded handles).

4. **Assets**  
   - Before or after load: ensure assets referenced by the scene (meshes, materials, scripts) are loadable from asset root(s). No need to load everything up front if you have lazy loading.

5. **Run loop**  
   - Start scheduler; each frame: `World::Update(delta)` (or equivalent) so all systems run.  
   - Render, input, etc. as you already do.

That’s the **unified loader**. The same exe can then be:

- **Unified runtime**: launched with any `--scene` + `--assets` to run any game.
- **Unity-style shipped**: same exe packaged with a default scene and assets so it runs one specific game.

---

## 6. One-line summary

- **Unified runtime** = one engine + one loader that runs any game from scene file + assets.  
- **Unity-style shipping** = building/shipping that same runtime with a specific game’s scene + assets.  
- **In your setup**: Same world module, same `Serializer`, same scheduler; add one **unified loader** (entry point that loads scene into `WorldState` and runs the loop). After that, “unified runtime” and “Unity-style shipping” are just different ways of **invoking and packaging** the same thing — **no architectural difference**.
