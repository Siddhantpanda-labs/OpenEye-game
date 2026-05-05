# MVP AI — Roadmap, architecture & tool loop

**Purpose:** Single narrative for **what we build**, **in what order**, and **how the LLM ↔ world loop works**. Engineering checklists and build flags also live in [`AI_PLAYGROUND_ROADMAP.md`](AI_PLAYGROUND_ROADMAP.md); keep milestones in sync when either doc changes.

| Field | Value |
|--------|--------|
| Version | 1.0 |
| Last updated | 2026-04-09 |
| Companion | [`AI_PLAYGROUND_ROADMAP.md`](AI_PLAYGROUND_ROADMAP.md), [`World_kernel_Ai.md`](../World_kernel_Ai.md) |

---

## 1. Vision (what “done” looks like for MVP)

The AI is a **controller**: it receives natural language, **observes** the world, **plans**, and applies **only** validated world operations until the user’s goal is met (or limits hit).

**Target capability (MVP Phase 1 — product slice):**

- User describes **small playable setups**, e.g.:
  - Minimal **shooter**: player, target (moving or stationary), **fire** at target → disappear → **respawn**
  - **Moving platform** platformer slice
  - **Flappy-bird**-style slice (where engine/runtime support exists)
- User can ask for **materials**, **colors**, **lights**, and scene layout changes.
- The model receives a **full catalog of tools/commands** (and examples); the team can **upload / version** that corpus (system prompt attachment, repo file, later vector search).

**Non-goals for this MVP doc:** replacing the main editor/player as primary product; full World Kernel v1 (see `World_kernel_Ai.md`).

---

## 2. Control architecture (three components)

| # | Component | Role |
|---|-----------|------|
| **A** | Instruction / intent | User text + context → structured goals (today: LLM + system prompt + tools) |
| **B** | Planner | Ordered **tool calls** (each maps to one or few world ops) |
| **C** | World bridge | Execute on `api::world::World`, **query** state, return **structured success/errors** |

```
Human: "Minimal shooter with respawning targets"
        ↓
     AI (A + B)
        ↓
  Tool calls → World commands / SetComponentJson / temporary commands
        ↓
     Scene + runtime behavior
```

---

## 3. Engineering rules (where code lives)

| Layer | Rule |
|--------|------|
| **`ai_playground` (client)** | Default place for **orchestration**: HTTP to LLM, agent loop, UI, mapping tool names → API calls, rich observation text for the model, allowlists at the client boundary. **No `eyelib/` includes** from the app — **`api/`** only. |
| **`api::world` + world commands** | All **mutations** that change simulation truth must go through the **same command discipline** as today (validate → apply → auditable path). |
| **`eyelib/` (library)** | **No change until necessary.** When required, prefer **architectural** fixes (clear boundaries, reuse), not one-off hacks in random subsystems. |
| **`temporary_cmd` (world module)** | New **mutation commands** that do not exist yet (e.g. material helpers, script attachment, input binding **if** not expressible via current API) live in a **dedicated world-side file** (e.g. `TemporaryWorldCommands.cpp` / paired header), implemented **like other commands** (same patterns, tests where feasible). After MVP, **merge** into permanent command types or **delete** per a written retire list. |

**Principle:** The playground **never** implements game rules by mutating EnTT/registry directly; it only calls **public world entry points** and command factories the world module exposes.

---

## 4. Phased roadmap and steps

Use checkboxes in your issue tracker or in [`AI_PLAYGROUND_ROADMAP.md`](AI_PLAYGROUND_ROADMAP.md); below is the **intended sequence**.

### Phase 0 — Foundations (complete)

- App target **`OpenEyeAIPlayground`**, CMake option, secrets hygiene, README.
- **Exit:** configure + build + run; no keys in repo.

### Phase 1 — MVP product slice: see → reason → build (+ play)

**Objective:** End-to-end **natural language → tools → world**, with **observation** so the model can iterate. Deliver **one vertical first**, then expand genres.

**1A — Observation & catalog**

- [ ] **Rich “world snapshot” for the model** (bounded): entities, names, key components, lights, materials in use — text and/or JSON built **in `ai_playground`** from existing `api::world` getters.
- [x] **Versioned tool catalog** in repo: [`tools_catalog_v1.md`](tools_catalog_v1.md) — slash commands, `/plan` ops, builtin meshes, examples (extend as tools grow).
- [ ] **Wire catalog into runtime**: load file and prepend to system prompt, or attach for OpenAI file search later.

**1B — Executor & safety (client)**

- [ ] Single **tool executor** mapping each tool → `World` calls (reuse/extend `WorldToolBridge` pattern).
- [ ] **Max tool rounds**, timeouts, **Cancel**; chat shows **tool + structured result** (not only prose).
- [ ] **Allowlist** component names / asset IDs where practical (bridge + engine validation).

**1C — LLM integration (when 1A–1B are stable)**

- [ ] HTTP client (TLS), env key, agent loop: user message → model → tool calls → execute → tool results → repeat until done or cap.
- [ ] Map tools to provider **function/tool JSON Schema**; version schema alongside catalog.

**1D — First playable vertical (pick one first)**

Example order (adjust to engine reality):

1. [ ] **Static scene + lights + materials** (crate/plane/light reference scenario extended).
2. [ ] **One interaction template** (e.g. destroy target + respawn) using **existing** Script/Lua/prefab path **if** available; else add **minimal** `temporary_cmd` + runtime hook.
3. [ ] Second genre only after (2) is repeatable.

**World module — `temporary_cmd` (add only when needed)**

- [ ] For each missing mutation: implement **command class** (or factory) in world module temporary file, **same style** as existing commands.
- [ ] Document each command in the **tool catalog**; add **retire list** when promoting to permanent API.

**Exit criteria (Phase 1):**

- User can prompt for **at least one** full mini-loop (see §1 vision) without hand-editing the scene in editor.
- **100%** of writes go through `api::world` / registered commands.
- Catalog is **shippable** and **versioned**; team knows how to refresh it after engine changes.

### Phase 2 — Bridge hardening (can overlap late Phase 1)

- [ ] Stricter allowlists, rate limits per session.
- [ ] **Automated replay**: fixed tool-call sequence → assert world JSON / entity count (CI smoke, headless if available).

### Phase 3 — Grounding at scale

- [ ] Vector store / file search over curated corpus (tool contracts, asset naming, examples).
- [ ] Process to refresh corpus when `api::world` or components change.

### Phase 4 — Release readiness

- [ ] Async LLM + **main-thread** world apply queue; profiling.
- [ ] Optional save-after-session; runbook (keys, outage, quota); changelog/version tag for internal beta.

---

## 5. Reference scenario (prove the loop early)

Keep a **small** scripted acceptance target (repeatable):

> *Place a wooden crate on a flat plane with a directional light; optional: assign material/color.*

Then escalate to **1D** playable verticals. If the reference scenario is **reliable** (N/M runs), the **control architecture** is proven before complex games.

---

## 6. Definition of done (MVP bundle — summary)

1. **Phase 1 exit criteria** (§4) met for at least **one** full user goal class + reference scenario.
2. **Observable** tool trace in UI; structured errors returned to the model.
3. **Secrets** only via environment; documented in README / roadmap.
4. **Automated replay** (Phase 2) for the executor path — target at least one test before calling MVP “closed.”

---

## 7. Risk register (short)

| Risk | Mitigation |
|------|------------|
| Model invents invalid JSON / IDs | Schema + allowlist + engine validation + error feedback loop |
| `temporary_cmd` becomes permanent debt | Retire list; merge or delete after verticals ship |
| UI thread blocked by LLM | Async HTTP; queue applies on world owner thread |
| Scope creep (three genres at once) | **1D** order: one vertical end-to-end before the next |

---

# Part II — Tool use (implementation pattern)

The sections below describe **why** tool calling beats ad-hoc JSON from the model, and a **reference** execution shape (pseudocode). **OpenEye** implements the loop in C++ (`ai_playground` + `api::world`); the JavaScript is illustrative.

---

## Tool use — core idea

Instead of this messy approach:

```
LLM → outputs text → you parse JSON → hope it's valid
```

You do this:

```
LLM → calls typed functions → you execute directly
```

You define your World Module operations as **tools** with schemas. The LLM proposes **tool calls**; your bridge executes them on `api::world` and returns results until the task completes or hits limits.

---

## How tool use works (the loop)

```
1. You send: goal + tool definitions (+ optional catalog file)
         ↓
2. LLM responds: tool_use (name + arguments)
         ↓
3. You execute that tool on World / commands
         ↓
4. You send structured result back to LLM
         ↓
5. LLM calls next tool or finishes
         ↓
6. Repeat until done or cap
```

---

## Step 1 — Define your tools

Every world-level operation becomes a tool definition (names, descriptions, JSON Schema / enums). Example shape (illustrative):

```javascript
const WORLD_TOOLS = [

  {
    name: "CreateEntity",
    description:
      "Creates a new entity in the simulation world. " +
      "Call this first before adding any components.",
    input_schema: {
      type: "object",
      properties: {
        name: {
          type: "string",
          description: "Unique name for this entity"
        },
        type: {
          type: "string",
          enum: ["static_mesh", "light",
                 "camera", "trigger", "empty"],
          description: "Type of entity to create"
        }
      },
      required: ["name", "type"]
    }
  },

  {
    name: "AddComponent",
    description:
      "Adds a component to an existing entity. " +
      "Entity must be created first.",
    input_schema: {
      type: "object",
      properties: {
        entity_name: {
          type: "string",
          description: "Name of the entity"
        },
        component: {
          type: "string",
          enum: ["Transform", "Mesh", "Light",
                 "Physics", "Collider", "Animation"],
          description: "Component type to add"
        },
        params: {
          type: "object",
          description: "Component parameters"
        }
      },
      required: ["entity_name", "component", "params"]
    }
  },

  {
    name: "SetComponent",
    description:
      "Modifies an existing component on an entity.",
    input_schema: {
      type: "object",
      properties: {
        entity_name: { type: "string" },
        component: {
          type: "string",
          enum: ["Transform", "Mesh", "Light",
                 "Physics", "Collider", "Animation"]
        },
        params: { type: "object" }
      },
      required: ["entity_name", "component", "params"]
    }
  },

  {
    name: "QueryWorldState",
    description:
      "Returns bounded world state for planning. " +
      "Call when you need to see what exists.",
    input_schema: {
      type: "object",
      properties: {},
      required: []
    }
  },

  {
    name: "DestroyEntity",
    description: "Removes an entity from the world.",
    input_schema: {
      type: "object",
      properties: {
        entity_name: { type: "string" }
      },
      required: ["entity_name"]
    }
  }

];
```

Map these conceptual tools to **real** `api::world::World` methods (`CreateEntity`, `SetComponentJson`, `GetComponentJson`, queries, and any **`temporary_cmd`** you add).

---

## Step 2 — System prompt (keep short)

Tools carry detailed API knowledge; the system prompt sets **global rules** (ordering, safety, allowlisted assets).

```javascript
const SYSTEM_PROMPT = `
You are a 3D world construction AI.
You build scenes by calling the provided tools.

Rules:
- Always CreateEntity before AddComponent
- Query world state if you need context
- Build incrementally
- Use only meshes/materials from the uploaded tool catalog
`;
```

---

## Step 3 — Execution engine (pseudocode)

Illustrative loop (language-agnostic pattern):

```javascript
class WorldAI {

  constructor(worldModule) {
    this.worldModule = worldModule;
    this.entityMap = {};
    this.conversationHistory = [];
  }

  async execute(userGoal) {

    this.conversationHistory.push({
      role: "user",
      content: userGoal
    });

    while (true) {

      const response = await this.callLLM();

      if (response.stop_reason === "end_turn") {
        return this.getResult();
      }

      if (response.stop_reason === "tool_use") {
        await this.processToolCalls(response);
      }
    }
  }

  async processToolCalls(response) {

    const toolResults = [];

    for (const block of response.content) {

      if (block.type !== "tool_use") continue;

      const result = await this.executeTool(
        block.name,
        block.input
      );

      toolResults.push({
        type: "tool_result",
        tool_use_id: block.id,
        content: JSON.stringify(result)
      });
    }

    this.conversationHistory.push({
      role: "user",
      content: toolResults
    });
  }

  async executeTool(toolName, params) {

    try {
      switch (toolName) {

        case "CreateEntity": {
          const id = this.worldModule.CreateEntity(params.type);
          this.entityMap[params.name] = id;
          return {
            status: "ok",
            entity_id: id,
            name: params.name
          };
        }

        case "AddComponent": {
          const id = this.entityMap[params.entity_name];
          if (!id) return {
            status: "error",
            message: `Entity '${params.entity_name}' not found`
          };
          this.worldModule.AddComponent(
            id,
            params.component,
            params.params
          );
          return { status: "ok" };
        }

        case "SetComponent": {
          const id = this.entityMap[params.entity_name];
          if (!id) return {
            status: "error",
            message: `Entity '${params.entity_name}' not found`
          };
          this.worldModule.SetComponent(
            id,
            params.component,
            params.params
          );
          return { status: "ok" };
        }

        case "QueryWorldState": {
          const state = this.worldModule.GetWorldState();
          return {
            status: "ok",
            world: state
          };
        }

        case "DestroyEntity": {
          const id = this.entityMap[params.entity_name];
          if (!id) return {
            status: "error",
            message: "Entity not found"
          };
          this.worldModule.DestroyEntity(id);
          delete this.entityMap[params.entity_name];
          return { status: "ok" };
        }

        default:
          return {
            status: "error",
            message: `Unknown tool: ${toolName}`
          };
      }

    } catch (err) {
      return {
        status: "error",
        message: err.message
      };
    }
  }
}
```

In **OpenEye**, `executeTool` is C++: map to `eyelib::api::world::World` and command submission; return JSON-serializable structs for the provider’s tool_result channel.

---

## Step 4 — Using it

```javascript
const ai = new WorldAI(yourWorldModule);

await ai.execute(
  "Create a stone floor with a wooden crate " +
  "in the center and a warm point light above it"
);
```

---

## Why tool use stays clean

| Problem with prose-only / free JSON | How tool use helps |
|-------------------------------------|---------------------|
| Invalid JSON | Schema + tool_use channel |
| Wrong param names | Enums + `required` |
| No feedback | Tool results each turn |
| Single shot | Agentic loop |

---

## Implementation order (maps to §4)

1. **Catalog + observation** (Phase 1A) — model must know what it can do and what exists.
2. **Executor + slash/debug parity** (Phase 1B) — same ops with or without LLM.
3. **LLM agent loop** (Phase 1C) — HTTP + caps + cancel.
4. **First vertical + `temporary_cmd` only as needed** (Phase 1D).
5. **Tests + grounding + ship** (Phases 2–4).

---

*End of `mvp_ai.md` — update version table when phases close; mirror milestone checkboxes in `AI_PLAYGROUND_ROADMAP.md` where appropriate.*
