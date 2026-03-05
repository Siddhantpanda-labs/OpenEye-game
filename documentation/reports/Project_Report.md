# OpenEye Project Report

**Purpose:** Summary of the project, chronology of work, achievements, current state, and ultimate goal.  
**Sources:** Repository documentation, `documentation/` plans, and git history.

---

## 1. What the project is

**OpenEye** is a **3D engine library** (`eyelib`) with a **simulation world** and **API layer** (`api/`), plus **Editor** and **Player** applications. It is built as:

- **Unified runtime** — One player executable loads any scene + assets (Unity-style: same exe, different data).
- **API-first** — Client code uses only `api/`; implementation lives in `eyelib/`.
- **World-centric** — ECS world with hierarchy, transforms, components, commands, serialization, and systems (physics, render, animation, scripting).

The vision is an **AI-first** programmable world (multiplayer VR metaverse, AI-controlled systems, admin-guided AI, future “movie director” mode). The current codebase is the engine and tooling foundation for that vision.

---

## 2. Ultimate goal (short)

- **Near term:** Stable, professional-grade engine: correct local/world transform separation, hierarchy, rendering, physics, editor, and player so that scenes run and edit reliably.
- **Long term:** A programmable, inspectable, modifiable universe where AI is a first-class agent: control systems, create/modify content, and (later) direct “movies” — with multiplayer VR and rich world interaction.

---

## 3. Chronology (from git, start → present)

| Phase | Commits / themes | Summary |
|-------|------------------|--------|
| **Seed** | Initial commit, Initial Steps | Project bootstrap. |
| **Rendering & content** | Model loading PBR, Post Processing, Animation Loading | PBR materials, post-processing, animation loading. |
| **Systems** | Physics, Particle, State Animation, Asset Pipeline | Physics system, particles, state machine, asset pipeline. |
| **Editor & content** | Camera controls (WASD/QE/X), build.bat, docking | Editor camera and docking. |
| **Fluids & simulation** | Particle and Fluid, Fluid smoke, softbody cloth | Fluids, smoke, cloth. |
| **Library refactor** | Refactoring library “more professional and cleaner” | Cleanup in render, animation, serialize. |
| **Testing & features** | Testing Eyelib, animation module, mesh ops, scripting | API tests, animation, mesh ops, scripting in library. |
| **Animation & lighting** | FPS movement, grounded animation, transitions, lighting/shadows | Animation polish, lighting and shadows. |
| **World & runtime** | “Overall Library Completion”, world mutation system, runtime loader/player | World mutation (commands), Phase 1 unified runtime/player. |
| **Data layout** | Partial/view/span, SoA, job pool (physics, animations) | SoA transforms, views, job pool for physics/anim. |
| **Render pipeline** | Render thread unified scene (Phases 0–5), RHI (OpenGL, Vulkan, bgfx, Diligent) | RHI layer, device injection, multi-backend; render thread plan. |
| **Editor & mesh** | Editor layer, scene in scene panel, Canonical Mesh Pipeline | Editor with scene view, mesh pipeline work. |
| **Physics & world** | SystemGraph scheduler, transform authority, Jolt backend, in-world physics | Jolt physics, system graph, transform authority. |
| **ECS/world merge** | “Refactor duplicate eyelib/ecs, all migrated to ecs/world” | Single world/ECS path; physics and hierarchy fixes. |
| **Render/transform fix** | Render module refactor, texture/material loading fix, hierarchical transform | Rendering and parenting fixes. |
| **Current (uncommitted/session)** | Transform: local vs world, GetWorldTransform/SetWorldTransform, gizmo world space, debug camera preview | Local/world API, editor gizmo and Game panel camera preview use world transform. |

*(Exact commit messages and hashes are in `git log --oneline --reverse` and `git log -80`.)*

---

## 4. What’s been achieved

### 4.1 Engine core (eyelib)

- **Rendering:** RHI layer (OpenGL primary; Diligent/bgfx tried). Forward and deferred, PBR, shadows, frame graph with DAG execution order. CPU/GPU split: mesh descriptor, GPU-only Mesh; texture/skybox with explicit device.
- **World:** ECS with EntityId, TransformStore (local TRS + cached world TRS), Hierarchy (parent/child, cycle-safe). TransformSystem: parent-before-child, TRS composition (no matrix decompose for reparent). Commands: Create/Destroy entity, SetTransform, Reparent (preserve world default), SetWorldTransform (world→local). Observation/snapshot for rendering.
- **Physics:** Jolt backend, rigid bodies, colliders, in-world systems, SystemGraph scheduler.
- **Animation:** Canonical skeleton, clips, job pool integration.
- **Scripting:** Lua, script components, property injection, entity userdata.
- **Audio:** OpenAL, 3D spatial.
- **Assets:** Model loading (Assimp), materials, asset registry, resolution from roots.

### 4.2 API layer (api/)

- **Stable API:** PIMPL; clients use only `api/`. World, Observation, Commands (SetTransform, SetWorldTransform, Reparent, etc.), render, physics, animation, assets exposed cleanly.
- **Transform API:** GetTransform (local), GetWorldTransform (world), SetTransform (local), SetWorldTransform (world→local).

### 4.3 Runtime & loader

- **Unified player:** One executable; `--scene`, `--assets`, config/profiles (dev/shipped). Load scene → bind assets → run loop. Exit codes for init/scene/asset/render/physics failures.
- **Bridges:** WorldRender, WorldPhysics, WorldAnimation, WorldScripting. Render builds scene from world snapshot (world transforms).

### 4.4 Editor

- **Scene view:** Renders world, ImGuizmo on selection. Gizmo uses **world** transform; apply writes back via **SetWorldTransform** (world→local). Local/World gizmo mode switch.
- **Game panel:** Play mode uses game runtime; when not playing, “Camera” dropdown previews an in-scene camera using **world** transform (parented cameras correct).
- **Hierarchy, Inspector, panels:** Entity list, selection, component inspection.

### 4.5 Architecture & docs

- **Design docs:** `Current_advice.md` (local/world, SetWorldTransform, gizmo rule). `Architecture_plan.md`, `Engine_Architectural_Standard.md` (hierarchy, transforms, systems bar). `Runtime_Player_And_Shipping.md`, `Neutral_In_Memory_Representation.md`, `Runtime_Loader_Check.md`.
- **Module docs:** `eyelib/world/WORLD_MODULE.md`, `eyelib/render/RENDER_MODULE.md` (structure, transform, render pipeline).
- **Player:** `Player_Runbook.md` (build, run, config, profiles).

---

## 5. Current state (as of this report)

- **World:** Local TRS authoritative; world TRS cached; TransformSystem + dirty propagation; Get/Set local and world; SetWorldTransform converts world→local; reparent preserves world by default.
- **Rendering:** Uses world transforms from snapshot; FrameGraph DAG; explicit device where needed; default forward shader `lit` (point/spot lights).
- **Editor:** Gizmo and debug camera preview use world transform; scene view and Game panel consistent with play.
- **Player:** Loads scene + assets, runs systems, renders; config/profiles; scripting, physics, animation wired.
- **Outstanding (from docs/plans):** Render thread phases 6–8; neutral in-memory representation; full Blender-like mesh ops; networking; VR; AI integration; “movie director” mode.

---

## 6. References (selected)

| Doc | Purpose |
|-----|--------|
| `documentation/intro.md` | Project overview, API rules, AI-first intent. |
| `documentation/plans/Current_advice.md` | Local/world transform, API, gizmo, SetWorldTransform. |
| `documentation/plans/Architecture_plan.md` | Hierarchy, transform store, professional bar. |
| `documentation/plans/Engine_Architectural_Standard.md` | Meta-standard for physics, rendering, networking, events. |
| `documentation/Runtime_Player_And_Shipping.md` | Unified runtime vs Unity-style shipping. |
| `documentation/Runtime_Loader_Check.md` | Loader pipeline, capabilities, rendering. |
| `documentation/Player_Runbook.md` | Build/run player, config, profiles. |
| `documentation/Neutral_In_Memory_Representation.md` | Format-agnostic world load path. |
| `eyelib/world/WORLD_MODULE.md` | World module layout, transform system, commands. |
| `eyelib/render/RENDER_MODULE.md` | Render module structure. |

---

*Report generated from repository documentation and git history. For exact commit chronology run `git log --oneline --reverse`.*
