# AI Playground — Product & Engineering Roadmap

**Purpose:** Track delivery of a **new client application** that uses **`EngineSource/api/`** only, combines a **live 3D scene** with a **chat/command UI**, and drives world changes through **LLM tool calls** mapped to **`api::world`** mutations. **Authoritative MVP definition:** **§1**.

**Companion docs:** [`mvp_ai.md`](mvp_ai.md) (tool-loop concept), repo [`README.md`](../../README.md), [`World_kernel_Ai.md`](../World_kernel_Ai.md) (long-term kernel vision).

**Document control**

| Field | Value |
|--------|--------|
| Version | 0.2 |
| Last updated | 2026-04-03 |
| Status | Active — execution |

---

## 1. MVP goal & control architecture (authoritative)

### 1.1 How to think about the AI

The AI is **not** a monolith. It is a **controller**: it takes a goal in natural language and translates it into **World Module** operations (commands / `api::world` entry points).

```
Human: "Create a forest scene with fog"
        ↓
     AI (controller)
        ↓
World Module commands  (CreateEntity, SetComponentJson, …)
        ↓
     Scene exists / updates
```

### 1.2 MVP task — small and provable

**Statement:** The AI receives a **text instruction** and produces a **valid, visible scene** using **only** World Module calls — **no human correction** in the loop.

**Reference scenario (acceptance demo):**

> *"Place a wooden crate on a flat plane with a directional light."*

The system must, via World Module only:

1. Create **plane** entity and attach what is needed for a visible **mesh** (and transform as required).
2. Create **crate** entity + **mesh** + **transform** (position on plane).
3. Create **light** entity + **directional light** parameters.
4. Return **success/failure** per step; on failure the controller can retry using **error feedback** (still no human editing the scene).

If this runs **reliably** (repeatable across runs with the same prompt class), the **control architecture is proven**. Training, richer games, and regions come **after**.

### 1.3 Minimal AI design (three components)

| # | Component | Role | Typical implementation (MVP) |
|---|-----------|------|------------------------------|
| **A** | **Instruction parser** | Natural language → structured intent (or direct tool plan) | Same process as LLM + system prompt + tool schema; optional separate “intent” JSON later |
| **B** | **Command planner** | Intent → **ordered sequence** of World Module operations | LLM tool calls (each tool = one or few commands) |
| **C** | **World Module bridge** | Execute calls, **query** state, return structured **success / errors** | C++ (or thin host) mapping tools → `api::world::World` |

**§4 Phase 2–3** implement **C** first (testable without network), then attach **A+B** via the LLM.

### 1.4 Contract: what the World Module must expose (MVP)

- **Catalog:** List of **allowed** commands/tools with **signatures** (names, parameters, JSON shapes for components).
- **Read:** Query **current world state** (entities, names, components — bounded payload).
- **Write feedback:** Every applied command returns **success or structured error** so the planner can correct.

Nothing else is required to **prove the loop**.

### 1.5 Honest starting point

- Use an **LLM** (Claude, GPT, or other) as the **planner** initially.
- Provide **tool definitions** + short system rules (create before add, allowlisted assets).
- **Execute** tool calls through the bridge; feed results back until the model stops or hits a cap.
- **Do not** train custom models for MVP — **prove the control loop first**; training / fine-tuning is out of scope until the loop is stable.

---

## 2. Goals & non-goals

### 2.1 Goals (MVP → v1)

- **G1 — Live authoring:** User types natural-language instructions; the **scene updates** on screen after validated world operations.
- **G2 — Correct integration boundary:** Client code includes **`api/`** headers only; all world writes go through **commands** / documented **`api::world::World`** entry points (e.g. `ApplyCommandSync`, `SetComponentJson` where appropriate).
- **G3 — Observable automation:** Chat shows **tool calls and outcomes** (success / structured errors), not only model prose.
- **G4 — Safe operation:** Bounded agent loop (max tool rounds, timeouts, allowlists); **no API keys** in binary, repo, or logs.
- **G5 — Grounding (optional v1):** OpenAI **file search / vector store** (or equivalent) so responses respect **project-specific** naming and conventions.

### 2.2 Non-goals (explicit)

- Replacing the **editor** or **player** as the primary shipping product.
- **Full** World Kernel v1 (semantic causality graph, multiplayer AI, movie director) — see `World_kernel_Ai.md` for that horizon.
- **Lua** as the primary AI control plane (secondary / human glue only, per architecture docs).

---

## 3. Engineering principles (production bar)

| Principle | Requirement |
|-----------|-------------|
| API boundary | No `#include` of `eyelib/` from the new app. |
| Mutation path | Prefer `ApplyCommandSync` / command factories / `SetComponentJson` — one consistent, auditable path. |
| Concurrency | LLM I/O on **worker/async** path; **apply** world mutations on the **thread that owns** simulation/world updates (match editor/player). |
| Secrets | `OPENAI_API_KEY` (or equivalent) from environment; never commit key files; rotate if exposed. |
| Failure modes | Timeouts, rate limits, and user-visible **Cancel**; tool errors as structured JSON back to the model. |
| Testability | Headless or minimal harness that can replay a **fixed tool-call sequence** against `api::world` (CI-friendly smoke). |

---

## 4. Phases & milestones

Use the **checklist** to track progress. Update this file as items complete (or move detailed tickets to your issue tracker and link them here).

### Phase 0 — Foundations & repo hygiene

**Objective:** Agree layout, CMake target, and safety baseline.

- [x] **P0.1** Add new app target: `EngineSource/app/ai_playground/` → **`OpenEyeAIPlayground`** (links **`OpenEyeGameRuntime`** + shared `PlayerConfig`).
- [x] **P0.2** CMake option **`OPENEYE_BUILD_AI_PLAYGROUND`** (root `CMakeLists.txt`, default **ON**). Set **OFF** in CI if you need a slimmer graph; no OpenAI dependency yet.
- [x] **P0.3** Repo **`.gitignore`**: patterns for `chatgpt.key.txt` and similar key filenames.
- [x] **P0.4** **`EngineSource/app/ai_playground/README.md`**: build, run, scene path, secrets note.

**Exit criteria:** Clean configure + build; app launches empty scene or loads default project; no secrets in tree.

---

### Phase 1 — UI shell: scene + chat

**Objective:** Production UX skeleton without LLM dependency.

- [x] **P1.1** Layout: **viewport** (full window) + **ImGui** chat column (fixed width right); stats corner overlay.
- [x] **P1.2** World tick + render via **`GameRuntime`** (same path as player).
- [x] **P1.3** Chat: multiline input, **Send**, scrollable history; stub assistant reply (no LLM).
- [x] **P1.4** Status: **LLM:** line in stats overlay (`Stub (Phase 3: OpenAI tools)`).

**Exit criteria:** User can type in chat (stub echo); scene renders. _Optional later:_ Clear/copy, resizable chat width, non-AI spawn test button.

---

### Phase 2 — Tool schema & executor (no network)

**Objective:** **Industry core** — implements **World Module bridge (§1.3-C)**; typed tools executed locally against `api::world`.

- [x] **P2.1** **Minimal bridge** in `app/ai_playground/WorldToolBridge.*`: create / destroy / get&set component JSON / query — exposed as **slash commands** in chat (maps to existing `api::world::World` methods only).
- [ ] **P2.2** **Allowlist:** component names, mesh/material IDs, max entities per message (bridge currently relies on engine validation only).
- [x] **P2.3** **User-visible outcomes** in chat (success / failure strings per command).
- [ ] **P2.4** Unit or integration test: script applies a fixed sequence; assert world state / JSON.

**Exit criteria:** Local “fake LLM” or test driver can drive the full mutation path; chat can invoke executor in debug mode. _(Slash commands satisfy local driving; automated test still open.)_

---

### Phase 3 — OpenAI integration

**Objective:** End-to-end agent loop (**instruction + planning via LLM §1.5**) with production safeguards.

- [x] **P3.1** HTTP client (TLS), timeouts — **WinHTTP** in `PlaygroundOpenAIHttp.cpp` (Windows). Non-Windows: stub error until portable client.
- [x] **P3.2** Map tools to OpenAI **tools** (JSON Schema) in `PlaygroundOpenAIAgent.cpp`; catalog still versioned in `tools_catalog_v1.md`.
- [x] **P3.3** Agent loop: `/ask` → Chat Completions → tool calls → `WorldToolBridge` → tool results → repeat until text or cap.
- [ ] **P3.4** **Cancel** flag; retries policy; optional async (currently **blocking** UI during `/ask`).
- [ ] **P3.5** Logging: redact secrets; optional verbose tool trace for dev builds only.

**Exit criteria:** Real model drives scene changes from chat; failures are handled gracefully and visible in UI. Delivers **§1.2 reference scenario** with high repeatability (define N-of-M pass rate in testing).

---

### Phase 4 — Grounding: vector store / file search

**Objective:** Model references **your** docs (asset naming, component field shapes, conventions).

- [ ] **P4.1** Curate **corpus**: small set of markdown/JSON (tool contracts, allowed assets, examples).
- [ ] **P4.2** Wire OpenAI **file search** (or Assistants + vector store — pick one API surface and stick to it).
- [ ] **P4.3** Document update process: how to refresh files after engine changes.

**Exit criteria:** Repeatable improvement in valid mesh/component usage vs Phase 3 in informal eval.

---

### Phase 5 — Hardening & release readiness

**Objective:** Ship-quality for internal or early external use.

- [ ] **P5.1** **Performance:** profile tool bursts; avoid blocking UI during HTTP (async + main-thread apply queue).
- [ ] **P5.2** **Persistence:** optional save scene after AI session (explicit user action).
- [ ] **P5.3** **Telemetry (optional):** anonymized usage counts, opt-in.
- [ ] **P5.4** **Runbook:** incident steps (key leak, model outage, quota).
- [ ] **P5.5** Version tag + changelog entry for first “AI Playground” release.

**Exit criteria:** Checklist signed off for your team’s definition of beta.

---

## 5. Risk register (living)

| ID | Risk | Mitigation | Owner | Status |
|----|------|------------|-------|--------|
| R1 | API key exposure | Env-only, pre-commit hook, audit git history | TBD | Open |
| R2 | Nondeterministic or unsafe world edits | Allowlists, max ops, command-only path | TBD | Open |
| R3 | UI thread blocked by LLM | Async HTTP, queued apply | TBD | Open |
| R4 | Unbounded context / cost | Token limits, conversation trim, user warning | TBD | Open |
| R5 | Model invents invalid component JSON | JSON Schema validation + error feedback loop | TBD | Open |

---

## 6. Open decisions (resolve and record)

| ID | Question | Options | Decision | Date |
|----|----------|---------|----------|------|
| D1 | App location & name | `app/ai_playground` vs `app/openeye_ai` | | |
| D2 | OpenAI API surface | Responses API vs Assistants + tools | | |
| D3 | Default project / assets | Ship minimal demo project vs require `Projects/...` | | |
| D4 | Networking dependency in CI | Build without OpenAI; mock driver only | | |

---

## 7. Progress log (append-only)

_Use this section for dated notes when closing phases or hitting blockers._

| Date | Note |
|------|------|
| 2026-04-03 | Roadmap created; Phases 0–5 defined. Next: Phase 0 (target + secrets hygiene). |
| 2026-04-03 | **§1 MVP goal** locked: AI as controller; three components (parser / planner / bridge); reference scenario plane+crate+light; prove loop before training. Doc v0.2. |
| 2026-04-06 | **Phase 0–1 (initial):** `OpenEyeAIPlayground` target; `OPENEYE_BUILD_AI_PLAYGROUND`; `ai_playground` ImGui UI + stub chat; `ApplyRuntimeDefaults` / `ApplyRuntimeProfile` moved to `PlayerConfig` for reuse; `.gitignore` key patterns. |
| 2026-04-06 | **Phase 2 (partial, ai_playground only):** `WorldToolBridge` + chat callback; `/help`, `/query`, `/create`, `/destroy`, `/get`, `/set` → `api::world` only. No changes to `api/`, `eyelib/`, editor, or player in this step. |
| 2026-04-09 | **Phase 1 (product slice, client):** `PlaygroundWorldObservation` + `/observe`; `/plan` JSON batch executor; `/ai demo` (builtin plane/cube/light); `documentation/Ai_plan/tools_catalog_v1.md`; UI/status strings updated. Still `api::world` only — no new world commands file yet. |
| 2026-04-09 | **Phase 3 (partial):** `/ask` + `PlaygroundOpenAIHttp` (WinHTTP) + `PlaygroundOpenAIAgent` (tools: observe, query text, run_world_plan); `ResyncFromWorld` after chat. Blocking UI; no Cancel yet. |

---

## 8. Definition of done (MVP bundle)

The MVP is **done** when **all** of the following hold:

1. **§1.2 reference scenario** succeeds without human scene edits: plane + crate + directional light, World Module only, with documented pass rate (e.g. N/M runs).  
2. Chat + viewport run in the new **`api/`-only** app.  
3. LLM tool loop applies **allowlisted** world ops; user sees **tools + results** in UI (§1.3 bridge + planner).  
4. Scene visibly updates after successful operations.  
5. Keys and secrets are handled per **§3** (Engineering principles).  
6. At least one **automated** test covers the executor path (**§4** Phase 2.4).

---

*End of roadmap — update checkboxes and §7 (Progress log) as work proceeds.*
