### OpenEye / OpenMeta — Current Status, Production Roadmap, and AI‑Driven Simulation Architecture

This document is the “single source of truth” for:
- **What we have today** in the OpenEye library and OpenMeta app.
- **What’s missing** to be production‑ready, scalable, and generic (2D + 3D).
- **How we will design OpenMeta** into a powerful simulation world driven and controlled by AI (with a safe, detailed control plane).

This is written from a “professional engine” perspective: stable architecture, clean ownership boundaries, data‑driven content, and scalability budgets.

---

### Table of contents
- **Where we are now**
- **Library vs App responsibilities**
- **OpenEye architecture (modules and contracts)**
- **Production readiness roadmap**
- **2D + 3D rendering plan (generic, professional)**
- **Animation system: where we are + what remains**
- **Large world simulation plan (streaming, networking, performance budgets)**
- **AI control plane design (how AI “drives the world”)**
- **Security, safety, determinism, and replay**
- **Milestones (what to build next in order)**

---

### Where we are now (summary)

OpenEye already contains multiple end‑to‑end systems working together:
- **Core runtime**: windowing, input, frame loop, logging.
- **Rendering**: deferred pipeline for opaque + transparency pipeline; post processing chain; framebuffer resize stability improvements.
- **ECS**: scene/registry, components and systems update order.
- **Physics**: Bullet rigid bodies + constraints; XPBD cloth (CPU); jelly blob approximation (rigid lattice + deform mesh).
- **Particles + Fluids**: GPU‑driven effects with fixes for depth/occlusion, hazards, presets stability, and editor integration.
- **Editor (ImGui)**: docking, panels, scene save/load; viewport issues handled.
- **Animation**:
  - legacy clip player still exists for simple playback
  - new “Unity/Unreal‑style” **AnimatorController** foundation exists (params, layers/masks, states/transitions, events)
  - an example Orc state machine (Idle/Walk/Run) is running in OpenMeta, and parameters can be edited in the Animator panel.

This is a strong foundation for a **real game/simulation**, but it is not yet “Unity/Unreal complete”.

---

### Library vs App responsibilities (non‑negotiable separation)

To stay professional and scalable:

- **OpenEye (library)** is the generic engine layer:
  - rendering, physics, animation, particles, fluids, assets, IO, job system, network primitives, scripting integration points
  - generic ECS components/systems and platform abstraction
  - should not contain “game rules” or specific world content

- **OpenMeta (app/client)** is the product:
  - “city sandbox / simulation world”
  - default scenes, gameplay logic, arenas, mission logic, AI integration, “world rules”
  - app‑specific systems that depend on a specific design

Rule of thumb:
- If it can be reused across many games → **OpenEye**
- If it describes “your world” → **OpenMeta**

---

### OpenEye architecture (what it should look like)

The library should be organized as coherent modules with stable APIs.
This matches the direction in `plan.md` and builds toward a production engine.

Example structure (conceptual):

```text
src/
  core/
    log/           window/        input/        time/        file/
    platform/      config/        jobs/         telemetry/
  ecs/
    Scene          Components      Systems       Prefab/Serializer
  rendering/
    resources/     passes/         post/         debug/
    scene/         rhi_gl/         rtt/          ui/
  animation/
    skeleton/      clips/          controller/   runtime/
  physics/
    world/         shapes/         bodies/       character/
  particles/
  fluids/
  assets/
    import/        cache/          registry/     hotreload/
  net/ (future)
  scripting/ (future)
```

Key contract boundaries we must keep clean:
- **Rendering** must not depend on editor UI
- **Simulation** must not call OpenGL (render thread separation)
- **Assets** must own loading/import/reload, not the gameplay code
- **Editor** reads and edits state, but does not “own” the runtime

---

### Production readiness roadmap (what’s missing to be “engine‑grade”)

We are “foundation‑complete” for many subsystems, but production readiness needs:

- **Threading and scheduling**
  - A real **job system** (worker pool, work stealing, frame graph tasks).
  - Clear thread ownership:
    - simulation update (jobs)
    - render submission (single render thread)
    - background IO (asset streaming)

- **Memory & performance budgets**
  - metrics and budgets: frame time slices per system
  - dynamic resolution, LOD scaling, simulation LOD
  - profiler (CPU and GPU), frame markers, and capture tools

- **Asset pipeline**
  - asset registry (GUIDs), import settings, caching, reimport
  - “cooked builds” for release
  - streaming assets in/out based on zones/chunks

- **Robust gameplay foundation**
  - input mapping layer
  - gameplay tags / layers (like Unity tags/layers)
  - scene streaming and deterministic spawn ordering
  - save/load that is stable across versions

- **Scripting / gameplay logic layer**
  - Lua or another scripting layer for iteration speed
  - strict bindings and sandboxing
  - hot reload for scripts (dev only)

- **Networking (if multiplayer or remote AI control)**
  - authoritative server model, replication, snapshots
  - entity replication rules and interest management

---

### 2D + 3D rendering (generic and professional)

We want OpenEye to be excellent at both 3D and 2D, without becoming messy.

#### Core principles
- **One renderer** (resource + GPU state + passes) shared by 2D and 3D.
- 2D is not “separate engine”: it is a set of passes and scene features.
- Everything uses the same:
  - shader/material system
  - texture system
  - batching/instancing
  - render graph / frame graph (later)

#### 3D rendering baseline
- Deferred for opaque (GBuffer), forward for transparency.
- Shadows (directional cascades), SSR/SSAO, bloom/tonemap.
- GPU culling / instancing for city scale (later).

#### 2D rendering baseline
2D should support:
- sprites, atlases, 9‑slice UI, text, tilemaps
- 2D lighting (optional) and post effects (optional)
- batching (texture‑sorted) and glyph caching

Implementation strategy:
- Introduce a `RenderScene` abstraction that can contain:
  - `MeshDraw`s (3D)
  - `SpriteDraw`s (2D)
  - `TextDraw`s (2D)
- Both feed into a shared render submission pipeline that sorts by:
  - pass → material pipeline state → textures → mesh/sprite batch

#### Professional organization and “no spaghetti”
We keep render responsibilities clear:
- `rendering/resources`: Mesh/Texture/Material/Shader/UBOs
- `rendering/passes`: GBuffer/Lighting/Transparency/Post/2DOverlay
- `rendering/scene`: visibility, draw lists, sorting, instancing data
- `rendering/debug`: wire, gizmo, overlays, GPU debug markers

---

### Animation system (where we are + what remains)

#### What we already achieved
- A clean controller asset concept:
  - params (float/int/bool/trigger)
  - states, transitions, blend times
  - layers and bone masks (foundation)
  - event tracks
- Runtime component + system + editor parameter tweaking
- A working example with Idle/Walk/Run transitions

#### What still needs to become “production ready”
- **Blend trees**
  - 1D blend space (speed)
  - 2D blend space (speed + direction)
  - proper normalization and deterministic evaluation
- **Retargeting**
  - map clips across skeletons (humanoid/IK rigs)
- **IK & constraints**
  - look‑at, foot IK, aim offsets
- **Additive animation**
  - upper body aim layer over locomotion
- **Graph editor**
  - visual authoring tool in the editor (later)

---

### OpenMeta (app) — what “powerful simulation world” means

OpenMeta is not “just a demo”. It should become a real product:
- a city‑scale world (exploration) + arenas (shooting) + puzzles
- later: VR, media screens, and AI world control

To keep it scalable, OpenMeta must be designed as:
- a **data‑driven world** (zones, rules, content definitions)
- a **runtime simulation** (tick‑based, budgeted)
- a **control plane** (AI, admin tools, server authority)

---

### AI‑driven world: the correct professional approach

The most important concept:
- AI should not “touch engine internals”.
- AI should use a **strict API** (commands + queries), with validation and budgets.

Think of the AI as a “client” of the simulation.

#### Architecture overview (control plane)

```text
             ┌──────────────────────────┐
             │  AI Agents (LLM/RL/etc.) │
             └───────────┬──────────────┘
                         │
              Commands / Queries (JSON)
                         │
             ┌───────────▼──────────────┐
             │   AI Control Service     │  (policy + rate limit + validation)
             └───────────┬──────────────┘
                         │
                 World Command Bus
                         │
             ┌───────────▼──────────────┐
             │     OpenMeta Runtime     │  (authoritative simulation)
             │  ECS + Physics + Rules   │
             └───────────┬──────────────┘
                         │
               Observations / Events
                         │
             ┌───────────▼──────────────┐
             │   AI Perception Layer    │  (summaries, sensors, world state)
             └──────────────────────────┘
```

#### Core components we need (AI integration)
- **WorldState / Snapshot**
  - stable data model of entities, zones, inventories, objectives, nav, etc.
  - versioned schemas

- **WorldQuery API**
  - safe reads: “get entities in radius”, “get navmesh region”, “get NPC state”
  - supports “compressed” summaries for LLM context windows

- **WorldCommand API**
  - safe writes: spawn, move, set target, build, destroy, set weather, trigger event
  - every command is validated against:
    - permissions (who is allowed)
    - budgets (how much per tick)
    - world rules (no impossible state)

- **Event Bus**
  - simulation emits events (combat hit, door opened, footstep event, etc.)
  - AI subscribes to relevant events only (interest management)

- **Policy + Budget**
  - prevent “AI goes crazy” and breaks simulation
  - rate limits by agent, zone, command type
  - rollback or command rejection on invalid actions

---

### AI control API (detailed and practical)

We want the AI to feel powerful, but stay safe and debuggable.

#### Command style
Commands must be:
- deterministic (or at least replayable)
- validated
- executed at specific simulation ticks

Example command envelope:

```json
{
  "tick": 123456,
  "agentId": "npc.orc_01",
  "scope": "zone.downtown",
  "idempotencyKey": "123456-npc.orc_01-move-001",
  "cmd": {
    "type": "MoveTo",
    "entity": "npc.orc_01",
    "target": { "x": 10.2, "y": 0.0, "z": -3.5 },
    "speed": 3.5
  }
}
```

#### Query style
Queries must be:
- bounded cost (limit results)
- optionally summarized (“LLM summary mode”)

Example query:

```json
{
  "agentId": "npc.orc_01",
  "query": {
    "type": "GetNearbyEntities",
    "centerEntity": "npc.orc_01",
    "radius": 25.0,
    "filter": ["Player", "Door", "CoverPoint"],
    "limit": 64
  }
}
```

#### Recommended “AI‑ready” engine components
To make commands meaningful:
- `AIControllerComponent` (goal/behavior state, blackboard, permissions)
- `NavAgentComponent` (path requests, steering)
- `InteractableComponent` (door, button, pickup, terminal)
- `Faction/TeamComponent` (hostile/friendly)
- `Health/CombatComponent`
- `Quest/ObjectiveComponent`

#### Animation + AI
Your animator parameters become part of the control interface:
- AI sets `speed`, `isAiming`, triggers `shoot`, etc.
- AI can observe events fired from animation (footsteps, reload timing).

---

### Safety, determinism, and replay (required for AI worlds)

If AI will “drive” the world, debugging must be first‑class:

- **Command log** (append‑only):
  - record every accepted command with tick, agentId, parameters
- **Event log**:
  - record major state changes and events
- **Replay mode**:
  - re‑run the world from logs to reproduce issues

Determinism levels:
- For offline single‑player AI: “mostly deterministic” is okay
- For multiplayer + AI: you need stronger determinism or authoritative server snapshots

---

### Are we “ready to build anything”?

Answer: **We’re ready to build a real product now**, and the library is strong enough to grow into a full engine over time.

But “full engine” readiness is a roadmap, not a single switch.
The correct approach is:
- build OpenMeta city sandbox (product)
- add engine features only when needed
- keep library boundaries clean so it stays reusable

---

### Milestones (recommended order)

#### Milestone A — Make OpenMeta a solid “game foundation”
- city exploration zone + multiple arena zones
- player controller, camera, basic combat loop
- save/load scenes robustly

#### Milestone B — Large world scalability
- world streaming (chunks), LOD and instancing budgets
- background asset streaming (IO thread)
- performance profiler (CPU/GPU)

#### Milestone C — Multiplayer baseline (if needed)
- authoritative server, snapshot replication, interest management

#### Milestone D — AI integration layer (control plane)
- command bus + query bus
- permissions + rate limiting
- structured world state summaries for AI
- replay & audit logs

#### Milestone E — VR + media screens
- VR camera + input abstraction
- video textures + RTT for in‑world screens

---

### Next decisions (you tell me, we implement)

To design the AI control plane correctly, we must pick the target deployment:
- **Single‑player offline** (AI runs locally)
- **Server authoritative** (AI services connect to a world server)
- **Hybrid** (local AI for NPCs, remote AI for high‑level director)

And we should decide “AI role”:
- **NPC control** (many agents controlling characters)
- **World director** (spawns events, changes weather, missions)
- **Builder** (procedural city changes and content generation)

Once you confirm these, we’ll implement:
- the World Command/Query schema
- the ECS components that expose control safely
- the logging/replay pipeline
