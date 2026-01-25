# World Kernel for AI-Operable Simulation

This document summarizes and structures the requirements in `AI_Sim.md` into a concise, actionable specification for a **World Kernel** that enables deep AI control, local game systems, and scalable evolution.

---

## 1. Goal

Build a **simulation world** (client side) where:

- Players can explore a large urban world (VR-ready).
- Local game arenas exist inside the same world (shooting, puzzles, etc.).
- AI can create, modify, and run gameplay systems inside the world.
- AI can progressively craft assets and animations in-world.
- The world is designed for **AI control first**, game second.

---

## 2. Core Reframe

The simulation world is **not a game** that AI controls.

It is a **programmable, inspectable, modifiable universe** where:

- Humans are just another type of agent.
- AI operates through structured world operations.
- All rules and behaviors are **data**, not hardcoded code.

---

## 3. World Kernel Properties (Mandatory)

The kernel must be:

- Deterministic stepping (fixed update order).
- Hot-editable at runtime.
- Fully introspectable.
- Fully serializable (snapshots, replay).
- Headless-capable (logic without rendering).

---

## 4. Layered Architecture (Strict Separation)

### Layer 1: World Kernel (untouchable)

- ECS storage
- Deterministic simulation loop
- Physics stepping
- Event system
- Serialization / snapshots
- Time control

AI cannot bypass this layer.

### Layer 2: World Logic Layer (AI playground)

Everything AI edits lives here:

- Rule graphs
- Behavior graphs
- Game graphs
- Templates
- Constraints

### Layer 3: Presentation Layer (human-facing)

- Rendering
- Audio
- VR input
- UI
- Cameras

This layer does not define reality.

---

## 5. World Representation

The world must be a **semantic graph**, not just ECS.

### Nodes

- Entity
- Component
- System
- Rule
- Script
- Asset
- Player
- AI Agent
- Region
- Animation
- Constraint

### Relations

- Entity has Component
- System operates on Component
- Rule modifies System
- Animation drives Transform
- Region scopes Rule/System

This graph is what AI reads and edits.

---

## 6. World Introspection API (AI READS)

AI must be able to query:

- Entities and components
- Active systems
- Rules affecting an entity
- Dependency graph of systems and rules
- Recent events and changes
- Cause of actions (why something moved or collided)

This is required for AI reasoning.

---

## 7. World Manipulation API (AI WRITES)

AI does not call engine internals. It issues **structured commands**:

- Create / destroy entity
- Add / remove component
- Set component data
- Enable / disable systems
- Create / modify / delete rules
- Attach logic to regions
- Spawn assets progressively
- Trigger animations

Commands are:

- Validated
- Batched
- Logged
- Applied at safe points

---

## 8. Region-Scoped Games (Key Mechanism)

There is **one global simulation world**.
Games exist as **local rule graphs** bound to regions.

### Example flow

```
CreateRegion("Arena_01")
AttachGameGraph("ShooterGame", Region_Arena_01)
```

- Inside region: special rules and systems apply.
- Outside region: normal simulation rules apply.

Multiple game modes can run simultaneously in different regions.

---

## 9. Logic as Data (No Lua as primary control)

Lua is optional, secondary, and local.

Primary control is **data-driven logic**:

- Rule graphs: IF -> THEN
- Behavior graphs: state machines
- System graphs: execution and dependency flow

AI edits graphs, not code.

---

## 10. Progressive Asset Creation (AI "Molding")

To support real-time asset creation:

- Geometry is editable data.
- Meshes can update incrementally.
- Materials update live.
- Versioned asset states support undo/redo.

Loop:

```
Describe -> Rough form -> Evaluate -> Refine -> Lock
```

This must happen inside the live world.

---

## 11. Safety, Authority, and Roles

Roles:

- Admin (full authority)
- AI Director (bounded authority)
- Player (limited actions)

Required:

- Action logging
- Reversible operations
- World snapshots
- Branching / rollback

---

## 12. Recording and Replay (Movie Director Goal)

To support AI-directed filmmaking:

- Deterministic replay
- Camera control independent of players
- Timeline control
- Event annotation

If the world is deterministic + serializable, this comes naturally.

---

## 13. Minimum World Kernel v0 (Execution Starter)

The smallest kernel that proves the concept:

1. Deterministic simulation loop (fixed step).
2. Minimal ECS (Transform, Collider, Controller, Health).
3. Region system (non-overlapping, non-nested).
4. Simple rule engine (IF -> THEN).
5. System scheduler (fixed order).
6. World command interface (AI control).
7. World snapshot (basic serialization).

No behavior trees, no visual editors, no constraints, no templates yet.

---

## 14. First "AI Creates a Game" Milestone

AI must be able to build a playable arena with:

- One region
- One player
- One enemy or target
- One interaction (hit / collide)
- Win and lose conditions

