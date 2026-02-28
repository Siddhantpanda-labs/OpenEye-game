# Physics Backend Plan — Interface, Backends, and Single Authority

This document describes the migration from the current dual-physics setup (Bullet + in-world systems) to a **single physics backend behind a clean interface**, with a **factory/registration system** for backends and **one write path** into the World.

---

## 1. Problem

- **Two physics systems** currently write the same transform data:
  - **eyelib/physics** (Bullet) → stepped by `WorldPhysicsBridge`, then `SyncTransformsToWorld(world)` writes into the World.
  - **eyelib/world/systems/** (PhysicsSystem, BroadPhase, NarrowPhase, CollisionResponse) → run inside `world.Update()` and also write transforms for entities with RigidBody.
- Whichever runs last “wins”; result is flicker and undefined behavior.
- Bullet types leak at include boundaries (e.g. `api/physics/CollisionShape.h` → `CollisionShapeFactory.h` with `btCollisionShape*`).
- Goal: **one physics backend**, chosen at startup or compile time, with no if/else chains; engine and client code talk only to an interface.

---

## 2. Target Architecture

### 2.1 Layering

```
Client (app/player, game code)
         │
         ▼
   api/physics/                    ← thin layer: exposes what clients need
         │
         ▼
   eyelib/physics/                 ← "our physics library" (interface only)
   ├── IPhysicsWorld.h             ← engine talks to this
   ├── PhysicsTypes.h              ← opaque handles, Vec3, Transform, Ray, etc. (no backend types)
   ├── RigidBody.h                 ← thin wrapper: handle + interface calls
   ├── CollisionShape.h            ← same
   ├── CharacterController.h      ← same
   │
   └── backend/
       └── jolt/                   ← Jolt implementation; JPH:: never leaves this folder
           ├── JoltBackend.h
           ├── JoltBackend.cpp
           ├── JoltBodyFactory.cpp
           ├── JoltShapeFactory.cpp
           ├── JoltVehicle.cpp
           └── JoltSoftBody.cpp
```

- **eyelib/physics** (above `backend/`) = interface + types only. No Jolt, Bullet, or PhysX headers.
- **eyelib/physics/backend/jolt/** = implements the interface using Jolt. All `JPH::` usage is confined here.
- **api/physics** = thin wrapper over eyelib/physics for the client; no backend types.

### 2.2 Backend Factory / Registration

Backend is selected at **startup** (or compile time) via a factory; no if/else chains in engine code.

```cpp
// Startup (e.g. in player or engine init)
enum class PhysicsBackendType { Jolt, PhysX };  // extend when adding backends

auto backend = PhysicsBackendFactory::create(PhysicsBackendType::Jolt);
PhysicsSystem::initialize(std::move(backend));
```

- **PhysicsBackendFactory** lives in eyelib/physics (or a small registration module). It knows how to instantiate each backend; backend-specific code is linked but not referenced by if/else in hot paths.
- **PhysicsSystem** (or the engine’s physics holder) holds `std::unique_ptr<IPhysicsWorld>` and forwards all calls. No backend type checks after init.

Optional: **compile-time** selection via CMake option (e.g. `OPENEYE_PHYSICS_BACKEND=Jolt`) so only one backend is linked. Factory then has a single implementation path per build.

### 2.3 Single Write Path

- **Only** the physics backend (via **WorldPhysicsBridge**) writes rigid-body transforms into the World.
- **WorldPhysicsBridge** talks to `IPhysicsWorld*`: step, get transforms, then `world.SetTransform(...)` (or equivalent) for each bound entity. No other system writes those transforms.
- **In-world pipeline**: Remove or disable **PhysicsSystem**, **BroadPhaseSystem**, **NarrowPhaseSystem**, **CollisionResponseSystem** (and **WorldAABBSystem** if used only for that pipeline) for the player/runtime so they do not run for physics-driven entities. Alternatively remove them entirely and rely on the backend for all rigid-body motion and collision.

---

## 3. Interface Contract (eyelib/physics)

- **IPhysicsWorld**: create/destroy bodies, get/set transform and velocity, step, raycast, sweep, overlap, etc. All using **PhysicsTypes** (handles, Vec3, Transform, Ray, RaycastResult, etc.).
- **PhysicsTypes.h**: only engine types and opaque handles; no `JPH::`, `bt*`, or `Px*`.
- **RigidBody / CollisionShape / CharacterController**: thin wrappers that take `IPhysicsWorld&` (or a world reference) and a handle; they call interface methods. No backend types in their headers.

Rule: **Nothing above `backend/<name>/` includes a backend’s headers.** For Jolt, `JPH::` types never escape `backend/jolt/`.

---

## 4. How Existing Code Fits

| Current | After migration |
|--------|------------------|
| WorldPhysicsBridge | Holds/builds from `IPhysicsWorld*` (from factory). Step + SyncTransformsToWorld unchanged in behavior; only the object it calls is the interface. |
| api/physics (PhysicsWorld, RigidBody, etc.) | Thin layer over eyelib/physics: forwards to `IPhysicsWorld` and PhysicsTypes. No backend includes. |
| eyelib/physics/PhysicsWorld.cpp (Bullet) | Replaced by backend/jolt/JoltBackend.cpp. Bullet implementation can live in backend/bullet/ temporarily or be removed. |
| Vehicle, SoftBody | Move into backend/jolt/ (JoltVehicle.cpp, JoltSoftBody.cpp); they use JPH:: only there. |
| CollisionShapeFactory (eyelib) | Becomes backend-specific (e.g. JoltShapeFactory in backend/jolt/). api/physics no longer includes a header that declares `btCollisionShape*`. |

---

## 5. Migration Steps (Order of Work)

1. **Define interface and types**
   - Add `IPhysicsWorld.h` and `PhysicsTypes.h` in eyelib/physics (no backend includes).
   - Define handles, RigidBodyDesc, Transform, Ray, RaycastResult, etc., and the interface methods the engine and bridge need.

2. **Add backend factory**
   - Add `PhysicsBackendType` enum and `PhysicsBackendFactory::create(BackendType)`.
   - Wire so that at startup a backend is created and passed to the engine’s physics holder (e.g. PhysicsSystem::initialize(std::move(backend))).

3. **Implement Jolt backend**
   - Add eyelib/physics/backend/jolt/ with JoltBackend implementing IPhysicsWorld.
   - Add JoltBodyFactory, JoltShapeFactory; move or reimplement Vehicle and SoftBody as JoltVehicle, JoltSoftBody inside backend/jolt/.

4. **Point bridge and api at interface**
   - WorldPhysicsBridge uses IPhysicsWorld* (from factory).
   - api/physics implementation calls into IPhysicsWorld and PhysicsTypes only; remove includes of Bullet-backed factories from api headers.

5. **Single write path**
   - Ensure only the bridge syncs transforms into the World after physics step.
   - Remove or disable in-world PhysicsSystem, BroadPhase, NarrowPhase, CollisionResponse (and related WorldAABB if only for that pipeline) so they do not run for physics-driven entities, or remove them entirely.

6. **Remove Bullet from the default path**
   - Once Jolt backend is default and tested, remove or isolate Bullet (e.g. backend/bullet/ optional or deleted). No engine or api code branches on “if Bullet” in hot paths.

7. **Optional: compile-time backend selection**
   - CMake option to choose backend; factory only links/registers the chosen backend. Reduces binary size and removes runtime backend switch if not needed.

---

## 6. Future Backends

To add another backend (e.g. PhysX):

```
eyelib/physics/backend/
├── jolt/
│   └── ...
└── physx/
    ├── PhysXBackend.h
    └── PhysXBackend.cpp
```

- Same `IPhysicsWorld` and `PhysicsTypes`.
- Factory extended: `PhysicsBackendFactory::create(BackendType::PhysX)`.
- No changes to engine or client code beyond choosing the backend at startup (or compile time).

---

## 7. What’s Done

Track progress here. Update as items are completed.

| # | Item | Status |
|---|------|--------|
| 1 | IPhysicsWorld.h and PhysicsTypes.h added in eyelib/physics (no backend includes) | ☑ Done |
| 2 | PhysicsBackendType enum and PhysicsBackendFactory::create() implemented | ☑ Done |
| 3 | Engine/physics holder accepts and uses IPhysicsWorld only (bridge holds backend from factory) | ☑ Done |
| 4 | backend/jolt/ added: JoltBackend implements IPhysicsWorld (stub; real Jolt integration TBD) | ☑ Done |
| 5 | JoltBodyFactory, JoltShapeFactory in backend/jolt/ (or in JoltBackend; real Jolt shapes/bodies TBD) | ☐ Not started |
| 6 | Vehicle and SoftBody moved/reimplemented in backend/jolt/ (JoltVehicle, JoltSoftBody) | ☐ Not started |
| 7 | WorldPhysicsBridge uses IPhysicsWorld from factory; single write path (bridge only syncs transforms) | ☑ Done |
| 8 | api/physics thin layer over IPhysicsWorld + PhysicsTypes; no backend/Bullet includes in api headers | ☑ Done |
| 9 | In-world PhysicsSystem, BroadPhase, NarrowPhase, CollisionResponse disabled or removed for physics-driven entities | ☑ Done |
| 10 | Bullet removed from default path or isolated to optional backend/bullet/ | ☐ Not started |
| 11 | (Optional) Compile-time backend selection via CMake | ☐ Not started |

**Current status (summary):** The backend-agnostic path is in place: **api/physics** exposes `PhysicsWorld` and `IPhysicsWorld` (no Bullet/types in api headers). **eyelib/physics/backend/jolt** implements `IPhysicsWorld` (JoltBackend stub). Player and client code use only api; Vehicle/Constraint/CharacterController/SoftBody/Query are api-only stubs. **In-world manual physics removed:** PhysicsSystem, WorldAABBSystem, BroadPhase, NarrowPhase, CollisionResponse have been removed from eyelib/world; only TransformSystem and RegionSystem run in simulation. Physics is done solely by eyelib/physics (Jolt); WorldPhysicsBridge steps the backend and syncs transforms. **Remaining gaps:** (1) **eyelib/ecs/World** still has `SetPhysicsWorld`/`GetPhysicsWorld(eyelib::physics::PhysicsWorld*)` (Bullet type); (2) **eyelib/ecs/systems/PhysicsSystem** uses that world for transform sync—when using the Jolt path this system is redundant with WorldPhysicsBridge; (3) Bullet/legacy code still in tree (eyelib/physics Bullet wrapper, not on default player path).
