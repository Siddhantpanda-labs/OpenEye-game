# Physics Authority Model

This document defines how physics integrates with the SystemGraphScheduler and the Transform layer. It is the contract that prevents hidden updates and preserves determinism.

---

## 1. Authority Choice: Physics-Authoritative

We use **Physics-Authoritative** motion:

* **Transform local** is input to physics at the start of each fixed substep (world matrices from previous TransformSystem run).
* **Physics owns** world-space motion integration.
* At the end of each fixed substep:
  * Physics writes **new local transforms** (position/rotation from integration).
  * **TransformSystem** runs again to recompute world matrices from the updated local state.
* No transform mutation outside the Simulation phase.

TransformSystem owns **world matrix derivation**. Physics owns **motion integration** and **writing back local transform**. Both are Simulation-phase systems; no hidden updates.

---

## 2. Required Simulation Order (per substep)

Inside the fixed-step loop, execution order must be:

```
1. TransformSystem         — recompute world from current local
2. PhysicsSystem          — step simulation (read world, integrate, write local)
3. TransformSystem        — recompute world again (TransformSystemPostPhysics)
```

TransformSystem is idempotent and runs twice per substep: once before physics (so physics reads correct world matrices), once after physics write-back (so the rest of the frame sees updated world matrices).

**PostPhysicsStep hook:** After each substep (after TransformSystemPostPhysics), the scheduler invokes an optional **per-substep** callback. Use it to **enqueue events only** (e.g. collision/contact); it must **not** mutate simulation state. Events are then consumed frame-level (e.g. in PostSimulation) or by the app. This keeps collision callbacks deterministic (per substep, not tied to frame rate).

---

## 3. Rules Enforced

* **PhysicsSystem** runs only in the Simulation phase.
* No physics writes outside the fixed-step loop.
* Commands submitted during physics apply **next frame** (ProcessCommands runs once per frame, before PreSimulation).
* No transform writes outside the Simulation phase.
* No direct world-matrix writes by physics—only **local** position/rotation/scale. TransformSystem is the only writer of world matrices.

---

## 4. Physics Authority Contract (summary)

| Rule | Meaning |
|------|---------|
| Physics owns motion integration | Velocity → position integration is done by PhysicsSystem. |
| TransformSystem owns world matrix derivation | Only TransformSystem writes world matrices; physics writes local transform only. |
| Write-back before next transform recompute | Physics writes local; then TransformSystem runs again in the same substep. |
| All state mutation inside Simulation phase | No transform or physics mutation in ProcessCommands, PreSimulation, PostSimulation, or PreRender. |

---

## 5. Minimal Stub (before full collision)

The initial PhysicsSystem stub:

* Iterate entities with **RigidBody** (and Transform).
* Read local position and **linearVelocity** (on RigidBody).
* Integrate: `position += linearVelocity * dt`.
* Write new position to TransformStore (local), then **MarkLocalDirty**.
* No collision, no events, no interpolation yet.

This validates fixed-step order, transform recompute, and determinism under different frame rates.

---

## 6. Determinism Test

Run simulation for a fixed duration (e.g. 5 seconds) at:

* 60 FPS (frame delta = 1/60)
* 144 FPS (frame delta = 1/144)

With the same initial state and fixed timestep (e.g. 1/60), the **final transform positions** (or a hash of them) must be identical. If they differ, the authority boundary or fixed-step usage is wrong.

---

## 7. Per-Substep Event Hook (in place)

* **SetPostPhysicsStepCallback(fn)** on the scheduler (or World) runs **after** each Simulation substep (after TransformSystemPostPhysics), **inside** the fixed-step loop.
* The callback must **not mutate simulation state**—only enqueue events (e.g. collision/contact). Enforced by review.
* Collision/event dispatch can then run in this hook and push to a queue; consumers run later (frame-level or next frame).

---

## 8. In Place: World AABB and Contact Events

* **World AABB propagation:** WorldAABBSystem runs in Simulation after TransformSystemPostPhysics. It builds local AABBs from AABB or Collider (Box/Sphere/Capsule), transforms by world matrix, and writes the derived **WorldAABB** component. No serialization of WorldAABB.
* **Per-substep contact enqueue:** After each substep, the PostPhysicsStep callback runs. The default implementation (**EnqueueSubstepContacts**) reads `Components<WorldAABB>()`, does pairwise AABB-overlap tests, and enqueues **ContactEvent** (entityA, entityB) into a queue. Events are drained by the app via **GetAndClearContactEvents()** (eyelib World and api::world::World). No simulation state is mutated in the hook; collision response and narrow phase are not implemented here.

## 9. In Place: Collision Response and Contact Listeners

* **CollisionResponseSystem** runs in Simulation after WorldAABBSystem. In one pass it: finds AABB overlaps, enqueues **ContactEvent**(entityA, entityB) into the World queue, and applies **velocity response** (impulse along contact normal from AABB centers, restitution 0.5). Only entities with **RigidBody** are modified; kinematic bodies are skipped. Contact detection and enqueue no longer run in the PostPhysicsStep callback (callback is unset by default).
* **Contact event consumption:** Apps can poll **GetAndClearContactEvents()** or register **AddContactListener(callback)** and call **DrainAndDispatchContactEvents()** each frame (e.g. after Update) to have each contact event delivered to all listeners. Available on both eyelib World and api::world::World.

## 10. In Place: Narrow Phase (AABB) and Interpolation Support

* **Narrow phase (AABB):** CollisionResponseSystem uses the **minimum penetration axis** to choose the contact normal: overlap depths on X, Y, Z are computed; the axis with smallest overlap defines the normal (sign from A center to B center). This gives correct separation direction for box-vs-box and avoids wrong normals when centers are close.
* **Interpolation:** The scheduler exposes **GetAccumulator()** (SystemGraphScheduler, WorldScheduler); api::world::World exposes **GetSimulationAccumulator()**. The app can compute `alpha = 1 - GetSimulationAccumulator()/GetFixedTimestep()` and lerp between previous and current transform (or other state) for smooth rendering. No engine-stored "previous" state—the app (or a system) retains it and blends using alpha.

## 11. In Place: Geometry Narrow Phase and Engine Interpolation

* **Geometry narrow phase:** CollisionResponseSystem uses shape-specific contact when both entities have a **Collider**: **Sphere–Sphere** (center distance, normal A→B, depth = rA+rB−dist), **Sphere–Box** (closest point on AABB to sphere center, normal and depth from penetration). Box–Box and Capsule use the existing AABB min-penetration axis. World-space sphere center/radius from transform and scale; normal used for impulse response.
* **Engine-managed interpolation:** At the start of each **Update(deltaSeconds)** the World copies current local transforms to an internal **previous** buffer. **GetInterpolatedTransform(entity, alpha)** returns lerp(previous, current, alpha) with position/scale lerp and rotation slerp. Use `alpha = 1 - GetSimulationAccumulator()/GetFixedTimestep()` (0 = previous frame start, 1 = current). Available on eyelib World and api::world::World. **Update()** (no args) does not copy previous, so interpolation is for timed updates only.

## 12. In Place: Capsule Narrow Phase and Contact Detail in Events

* **Capsule narrow phase:** CollisionResponseSystem now has **capsule–sphere** and **capsule–capsule** shape tests. Capsule is represented as a world-space segment (Y-axis in local, transformed) plus radius. Capsule–sphere uses closest point on segment to sphere center; capsule–capsule uses closest point between the two segments. Box–capsule and capsule–box still use the AABB min-penetration fallback.
* **Contact detail in events:** **ContactEvent** includes **normal** (A toward B), **penetrationDepth**, and **contactPoint** (world-space). Filled by narrow phase or by the AABB fallback (ContactFromAABB provides normal, depth, and overlap midpoint). Listeners and polling apps can use these for VFX, audio, or gameplay.

## 13. Per-Body Physics Properties (RigidBody)

* **RigidBody** carries physics material and state: **mass** (default 1.0), **restitution** (default 0.5), **friction** (default 0.3), **useGravity** (default true), **isKinematic** (default false). All serialized in WorldSerializationTraits.
* **Collision response** uses **combined restitution** (average of body A and body B) and **mass-weighted impulse** (heavier body moves less; invMass used). No hardcoded restitution; per-body values only.

---

## 14. Gravity

* **Gravity** is a **World**-owned vector (default **0, -9.8, 0**). Each substep, **PhysicsSystem** applies `velocity += gravity * dt` for every non-kinematic **RigidBody** with **useGravity == true**, then integrates position. **SetGravity(vec3)** and **GetGravity()** are on eyelib World and **api::world::World**.

---

## 15. Force and Impulse Accumulation

* **RigidBody** has **accumulatedForce** and **accumulatedTorque** (cleared after each substep). **PhysicsSystem** each substep: applies queued **AddForce**/**AddImpulse** (drains pending lists), then `velocity += (accumulatedForce/mass)*dt`, clears **accumulatedForce**, then gravity and position integration.
* **AddForce(entity, force, ForceMode)**: **Force** = add to accumulatedForce (applied as (F/m)*dt); **Acceleration** = add directly to velocity (ignores mass). **AddImpulse(entity, impulse)**: applied as `velocity += impulse/mass`. Both are **queued** and applied at the start of the next substep (safe from gameplay/AI). **ForceMode** and pending entry types in **PhysicsEvents.h**. Exposed on eyelib World and **api::world::World**.

---

## 16. Angular Velocity and Rotation Integration

* **RigidBody** has **angularVelocity** (rad/s). **PhysicsSystem** each substep: applies queued **AddTorque** (accumulatedTorque += torque) and **AddAngularImpulse** (angularVelocity += impulse/mass), then `angularVelocity += (accumulatedTorque/mass)*dt`, clears **accumulatedTorque**, then builds a rotation delta from axis-angle (angle = |angularVelocity|*dt, axis = normalized angularVelocity) and multiplies into local rotation. **AddTorque** and **AddAngularImpulse** are queued and applied at start of next substep. Exposed on eyelib World and **api::world::World**.

---

## 17. Split Collision into Three Systems

* **BroadPhaseSystem:** Input: all entities with WorldAABB. Output: overlap pairs (entityA, entityB) to **FrameCollisionBuffer** (cleared each substep). **Sweep-and-prune on X axis**: sort by AABB minX, sweep for overlaps; no narrow phase.
* **NarrowPhaseSystem:** Input: FrameCollisionBuffer pairs. Output: **ContactData** (entityA, entityB, normal, depth, contactPoint) to **FrameContactBuffer** (cleared each substep). Uses shape-specific tests (SphereSphere, SphereBox, CapsuleSphere, CapsuleCapsule, ContactFromAABB fallback). Skips pairs where depth ≤ 0.
* **CollisionResponseSystem:** Input: FrameContactBuffer. For each contact: mass-weighted impulse (combined restitution), enqueue **ContactEvent** to World. Skips kinematic bodies for impulse; still enqueues event.
* Buffers owned by World. Execution order: **BroadPhaseSystem → NarrowPhaseSystem → CollisionResponseSystem** (after WorldAABBSystem).

---

## 18. Interpolation — Only Copy Dirty (RigidBody) Transforms

* **CopyTransformsToPrevious** now copies only transforms for entities that have a **RigidBody** component. Static entities are not written to the previous buffer; **GetInterpolatedTransform** still returns current transform when no previous entry exists.

---

## 19. What Is Not in Scope Yet

* Nothing remaining from the original “not in scope” list; the physics authority pipeline, narrow phase (sphere, box, capsule), contact events with detail, and engine interpolation are in place. Further work (e.g. gravity, angular velocity, configurable restitution, or a full physics engine) is optional.
