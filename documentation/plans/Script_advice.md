# Scripting Professionalization Plan (Unity/Unreal-like)

This plan defines what to build so scripting is production-grade: safe, scalable, deterministic, editor-friendly, and usable for real gameplay (main character logic, world interaction, game rules).

---

## 1) Current State (from codebase)

What is already good:
- Lua script lifecycle is present in runtime (`OnStart`, `OnUpdate`, `OnDestroy`).
- Script component exists (`scriptName`, `isEnabled`, `needsReload`, `properties`) and is serialized in scenes.
- Runtime bridge loads, updates, reloads, unloads scripts (`app/player/scripting/WorldScriptingBridge.cpp`).
- Script-to-world API surface is already broad (transforms, components, entity create/destroy, hierarchy, tags, batch operations, physics velocity APIs).

Current practical gaps:
- No input API exposed to Lua yet (cannot do proper WASD/mouse-driven character control in script).
- Editor does not expose Script in Add Component popup (attach is not convenient in editor flow).
- Hot reload is partially implemented but not production-stable (file timestamp handling and policy hardening needed).
- Scripting safety and scale concerns still exist from assessment (typing, context isolation, validation consistency).

---

## 2) Target (Definition of Done)

After this plan:
- A designer can attach script in editor to a character entity and run gameplay logic without JSON/manual hacks.
- Scripts can read player input and drive movement/interaction through safe world APIs.
- Script reload is reliable and predictable in editor/player.
- Script API is documented, versioned, and stable enough for game teams.
- Runtime behavior is deterministic and safe under malformed scripts.
- Basic profiling and debugging support exists (at least logs + timings + clear runtime errors).

---

## 3) Principles (must hold)

- API-first: app/editor/player use `api/*`; script-facing operations call world mutation path only.
- One writer rule: script world changes still go through world API/commands (no bypass writes).
- Deterministic frame behavior: script update order and side effects are consistent.
- Fail-safe runtime: script errors do not crash the engine; they degrade gracefully with diagnostics.
- Incremental rollout: each phase ends with measurable acceptance tests.

---

## 4) Phase Plan

## Phase 0 - Baseline, tests, and freeze points
Scope:
- Add a scripting test scene with:
  - one main character entity,
  - one interactable entity,
  - one environment trigger.
- Add automated smoke test: load scene -> scripts start -> 300 frames update -> no crash/assert.
- Record baseline logs and frame timings.

Acceptance:
- Deterministic pass for smoke test in Debug and Release.
- Known-good sample scripts committed (`assets/scripts/...`).

---

## Phase 1 - Editor usability (attach/edit scripts properly)
Scope:
- Add **Script** to editor Add Component popup.
- Add Script component inspector UI:
  - `scriptName` field (path/id),
  - `isEnabled`,
  - `needsReload`,
  - key-value `properties` editing.
- Validate script path in inspector and show clear errors.

Acceptance:
- Script can be added/removed fully from editor UI.
- Property edits persist to scene JSON and are restored on load.
- No direct state mutation bypasses in editor path.

---

## Phase 2 - Input API for gameplay scripts (main character ready)
Scope:
- Expose script-safe input APIs (keyboard, mouse buttons, mouse delta, optional gamepad):
  - `Input.GetKey`, `Input.GetKeyDown`, `Input.GetKeyUp`,
  - `Input.GetMouseButton`, `GetMouseDelta`,
  - optional `GetAxis`.
- Wire input context per runtime frame before script update.
- Provide sample `CharacterController.lua` using input + physics velocity APIs.

Acceptance:
- Main character can move/jump/look from script in player and play-in-editor.
- Input behaves consistently at different frame rates.

---

## Phase 3 - Runtime robustness and reload policy
Scope:
- Harden reload policy:
  - explicit manual reload command,
  - optional auto-reload with proper file modified time checks,
  - preserve script properties across reload.
- Error isolation:
  - if one script fails, others continue,
  - per-entity script error state (disabled until manual re-enable optional).
- Improve lifecycle guarantees:
  - `OnStart` once per activation,
  - `OnDestroy` on unload/disable/world teardown.

Acceptance:
- Repeated edit/reload cycles are stable (no leaked handles, no double-start).
- Script exceptions are logged with script path + entity + stack trace snippet.

---

## Phase 4 - Safety and typing improvements
Scope:
- Replace/complete weak entity passing with safer representation and validation on every bound API call.
- Tighten binding context storage per VM/runtime instance (no unsafe global sharing behavior).
- Add argument validation helpers for Lua bindings (type checks, range checks, null checks).
- Ensure all script-initiated mutations use world API command path.

Acceptance:
- Invalid script arguments never crash runtime.
- Multi-script/multi-entity stress run remains stable.

---

## Phase 5 - API quality (Unity/Unreal-like ergonomics)
Scope:
- Finalize stable script modules:
  - `World`, `Input`, `Time`, `Physics`, `Debug` (minimum set).
- Naming and consistency cleanup (predictable method names, return shapes, error semantics).
- Add API docs and examples for each module.
- Add version marker for scripting API changes.

Acceptance:
- New gameplay script author can build a working behavior from docs without reading C++.
- No breaking rename churn inside the same milestone.

---

## Phase 6 - Observability and tooling
Scope:
- Add script profiler basics:
  - per-script update time,
  - call counts,
  - slow script warnings.
- Add debugging quality-of-life:
  - runtime script error panel in editor,
  - script reload status and last error.
- Add test coverage:
  - lifecycle tests,
  - binding contract tests,
  - serialization roundtrip for Script properties.

Acceptance:
- Teams can identify slow or broken scripts quickly.
- CI includes scripting regression tests.

---

## 5) Recommended Execution Order (practical)

1. Phase 0 (baseline)
2. Phase 1 (editor attach/edit)
3. Phase 2 (input for actual character gameplay)
4. Phase 3 (reload + robustness)
5. Phase 4 (safety/typing hardening)
6. Phase 5 (API polish/docs)
7. Phase 6 (profiling/tooling)

Reason: this unlocks gameplay value early, then hardens quality without blocking iteration.

---

## 6) Risks and mitigations

- Risk: reload introduces nondeterministic behavior.
  - Mitigation: make reload explicit by default; auto-reload optional with strict guardrails.
- Risk: broad Lua API increases crash surface.
  - Mitigation: central validation helpers + defensive wrappers for all bindings.
- Risk: editor/player behavior diverges.
  - Mitigation: share same runtime scripting bridge and test both entry points.
- Risk: performance regressions from script-heavy scenes.
  - Mitigation: early profiler counters and frame budget warnings.

---

## 7) Immediate Next Tasks (start now)

- [ ] Add Script component to editor Add Component popup.
- [ ] Add Script inspector UI fields (`scriptName`, enable/reload, properties).
- [ ] Add minimal `Input` Lua bindings.
- [ ] Create `assets/scripts/CharacterController.lua` sample.
- [ ] Create scripting smoke test scene and runner.

If these 5 are done, scripting is practically usable for main-character logic and gameplay iteration; remaining phases raise it to professional long-term quality.
