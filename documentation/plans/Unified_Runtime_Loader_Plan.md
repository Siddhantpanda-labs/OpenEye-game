# Unified Runtime Loader Plan (Ultimate Loader)

This plan defines the unified runtime loader: a single, stable entry path that can run any game/simulation by loading a scene file plus assets and executing systems on the world. It uses the safe `api/` layer rather than direct `eyelib/` includes.

---

## 1) Goals

- **Single entry pipeline**: init engine → load assets → load scene → run scheduler.
- **Data-driven runtime**: no game-specific logic in the loader; behavior comes from systems/scripts/data.
- **Stable API usage**: loader depends on `api/` contracts, not internal `eyelib/` headers.
- **Reusable for editor/AI**: same world runtime that editor or AI can drive via commands.
- **Shippable**: same executable can be packaged with different scenes/assets.

---

## 2) Non-goals (for this plan)

- A full editor UI (handled in the editor plan).
- A scripting language runtime change (loader only initializes what already exists).
- Asset pipeline or importer tooling (handled elsewhere).
- A new serialization format (use current JSON serializer).

---

## 3) High-level architecture

**Runtime Player (Executable)**
- Uses `api/core`, `api/world`, and other `api/*` modules.
- Owns the main loop and all runtime configuration.

**Engine/World Library (eyelib)**
- Provides systems, world model, scheduler, serializer.
- Loader never includes these headers directly; uses `api/*`.

**Data**
- Scene file (JSON) → `WorldState` via `api/world::Serializer`.
- Assets referenced by scene (meshes, textures, scripts, etc.).

---

## 4) Unified loader pipeline (authoritative)

```
Process start
  ↓
Parse config/args (scene path, asset roots, runtime flags)
  ↓
Init engine modules (render, physics, audio, scripting, etc.)
  ↓
Register asset roots
  ↓
Load scene into WorldState (Serializer::Load)
  ↓
Resolve/prepare assets referenced by the scene
  ↓
Start scheduler loop
  ↓
Systems update each frame (world evolves)
```

---

## 5) Runtime configuration surface (minimal v1)

Inputs:
- `--scene <path>`: path to scene JSON.
- `--assets <path>`: asset root (can be repeated).
- `--log <path>`: log file path.
- `--fps <number>` or `--fixed <seconds>`: scheduler settings.
- `--paused` or `--step`: start paused / step once.

Defaults (if args missing):
- `scenes/main.json`
- `assets/`
- `OpenEyePlayer.log`

---

## 6) API usage (safe layer)

Use these APIs only:
- `api/core` (logging, platform, window/app lifecycle)
- `api/world` (`World`, `Serializer`, `Commands`, `Observation`)
- `api/render`, `api/physics`, etc. as needed (if exposed)

**No direct `eyelib/` includes** in the loader.

---

## 7) Loader responsibilities (v1)

### 7.1 Initialization
- Initialize logging.
- Initialize core platform/window (if required by render).
- Initialize engine modules (render/physics/audio/scripting).

### 7.2 Content roots
- Register asset roots (filesystem paths).
- Validate that paths exist; log warnings if missing.

### 7.3 Scene loading
- Create `api::world::World`.
- Load scene into its internal `WorldState`:
  - `api::world::Serializer::Load(world.GetInternal()->State(), scenePath)` or equivalent safe API path.
  - If a safe API for loading into `World` does not exist, add one in `api/world` instead of accessing `eyelib` directly.

### 7.4 Asset binding
- Resolve asset references from the scene (mesh/material/script paths).
- Preload critical assets or set lazy-load hooks (based on existing asset system).

### 7.5 Run loop
- Compute delta time.
- Call `world.Update(deltaSeconds)` each frame.
- Handle window events and render submission.
- Support pause/step/replay if exposed by `api/world::World`.

---

## 8) Gaps to close (expected small additions)

To keep loader API-only, we may need small `api/` extensions:

- **World load helper**: `api::world::World::LoadScene(path)` or `api::world::Load(world, path)` wrapper.
- **Asset root registration**: public API to register search roots for assets.
- **App lifecycle**: standard `api::core::App` loop if not already defined.

These are **small wrappers**; avoid leaking `eyelib` internals into loader.

---

## 9) Error handling + logging

- Hard failures:
  - Cannot initialize core systems.
  - Scene file missing or invalid.
- Soft failures:
  - Missing assets → log warning, continue if possible.
  - Unknown components → log warning; skip or keep raw data.

All errors should go through `api::core::Log`.

---

## 10) Packaging/shipping behavior

The loader should support both:

1. **Unified runtime mode**  
   - Launch with `--scene` and `--assets`.
2. **Shipped game mode**  
   - No args → defaults (`scenes/main.json`, `assets/`).
   - You ship the exe with those defaults bundled.

Same executable for both modes.

---

## 11) Runtime presets (profiles)

Provide two standard presets to keep builds consistent:

- **Dev preset**: overlay + stats on, strict assets off.
- **Shipped preset**: overlay + stats off, strict assets on, fixed timestep enabled.

Suggested files:

- `app/player/configs/dev.json`
- `app/player/configs/shipped.json`

These are optional, but they make it easy to start the player in a known state:

```
OpenEyePlayer --config app/player/configs/dev.json
OpenEyePlayer --config app/player/configs/shipped.json
```

---

## 12) Phase-by-phase build plan (follow this order)

**Phase 0 — Confirm contracts (1–2 days)**
- Lock the **scene JSON schema** (entities, hierarchy, components).
- Confirm **asset reference** format (path vs ID).
- Confirm which systems must start in the runtime (render/physics/etc.).
- Confirm `api/` surfaces needed by the loader (no `eyelib/` direct use).

**Phase 1 — Minimal player skeleton (2–4 days)**
- New `app/player/` target with a clean `main.cpp`.
- Parse args: `--scene`, `--assets`, `--log`.
- Initialize logging and core platform/window.
- Initialize essential engine modules.

**Phase 2 — World boot (2–4 days)**
- Create `api::world::World`.
- Load scene into world via `api::world::Serializer`.
- Add a safe `api` helper if direct load into `World` isn’t possible.
- Validate: start, load scene, no crash.

**Phase 3 — Runtime loop (2–3 days)**
- Run frame loop and call `world.Update(delta)`.
- Hook input/events + render submission.
- Add pause/step controls (from `api/world::World`).

**Phase 4 — Asset binding (3–5 days)**
- Register asset roots.
- Resolve scene asset references (mesh/material/script) to runtime assets.
- Add missing-asset warnings; continue if possible.

**Phase 5 — Robustness + diagnostics (2–4 days)**
- Clear error logging for missing scene/assets.
- Optional debug overlay (fps, scene name, entity count).
- Graceful shutdown + resource cleanup.

**Phase 6 — Shipping mode (2–3 days)**
- Default paths when args are missing (`scenes/main.json`, `assets/`).
- Validate that the same exe runs in **unified** and **shipped** mode.

**Phase 7 — API hardening (ongoing)**
- Keep loader using only `api/` headers.
- Add small `api` wrappers if needed (no internal coupling).
  
## 13) Incremental milestones (compact view)

**M1: Minimal runnable loader**
- New `app/player` target with command-line parsing.
- Init logging + window.
- Load scene into world.
- Call `world.Update(delta)` each frame.

**M2: Asset resolution pass**
- Register asset roots.
- Resolve scene references (mesh/material/script).

**M3: Runtime flags**
- Pause/step/time-scale controls.
- Fixed timestep override.

**M4: Robust errors + telemetry**
- Clear error logs.
- Optional debug overlay (fps, scene name, entity count).

---

## 12) Deliverables

- `app/player/` new target with `main.cpp`.
- Minimal config/arg parsing.
- Uses only `api/*`.
- Documentation update: how to run with `--scene` and `--assets`.

---

## 14) Success criteria

- A single executable can run **any** scene + assets without recompilation.
- No loader code depends on `eyelib/` headers.
- World update loop is active and stable.
- Different games are just **data changes** (scene + assets).

