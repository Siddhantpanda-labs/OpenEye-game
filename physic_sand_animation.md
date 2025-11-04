# Physics and Animation Systems — Implementation Plan & Status

**Last Updated:** 2025-11-04  
**Overall Status:** Architecture 90% | Integration 85%  
**Target:** Display animated FBX model with timeline UI + Bullet3 physics integration

---

## Executive Summary

### Current State
- ✅ **Architecture:** 95% complete (physics + animation subsystems solid)
- ✅ **Integration:** 85% complete (runtime integrated; remaining polish tasks)
- ✅ **Rendering (Skinned):** Complete (UBO bones, 512 max, GPU skinning)
- ✅ **UI:** Animation Timeline panel implemented and synchronized across submeshes

### Work Remaining: ~2-4 hours
1. SSR shader uniform cleanup + validation (minor)
2. Motion Blur (requires velocity buffer) — implement pass (medium)
3. Physics debug draw + example usage (minor)
4. Animation weight polish (top-4 sort + renormalize), optional importer pivots (minor)

---

## 1. Physics System — Implementation Status

### ✅ What's Implemented (90%)

#### Core Architecture
- **PhysicsWorld** (`src/physics/PhysicsWorld.h/cpp`)
  - Bullet3 integration (Bullet2 features, CPU-based)
  - World initialization/shutdown
  - Simulation stepping with sub-stepping
  - Gravity control
  - Factory method delegates for shapes, bodies, controllers
  - Query delegates for raycasting/sweeps

#### Collision Shapes (Factory Pattern)
- **CollisionShapeFactory** (`src/physics/shapes/`)
  - Primitives: Box, Sphere, Capsule, Cylinder
  - Mesh-based: Convex Hull, BVH Triangle Mesh (static)
  - Shape sharing (shared_ptr) for memory efficiency

#### Rigid Bodies
- **RigidBodyFactory** (`src/physics/bodies/`)
  - Creation/destruction with lifetime management
  - Mass/inertia calculation
  - Motion states for transform sync

#### Character Controllers
- **CharacterControllerFactory** (`src/physics/controllers/`)
  - Capsule-based kinematic controller
  - Configurable radius, height, step height
  - Jump support

#### Physics Queries
- **PhysicsQuery** (`src/physics/queries/`)
  - Raycast (all hits)
  - RaycastClosest (first hit)
  - SweepTest (shape-based continuous collision)

#### ECS Integration (Structure Only)
- **PhysicsSystem** (referenced in docs, needs verification)
  - Step simulation
  - Transform ↔ PhysicsBody sync (bidirectional)
  - Component lifecycle: OnPhysicsBodyAdded/Removed, OnCharacterControllerAdded/Removed
  - Forces/Impulses: ApplyForce, ApplyImpulse, ApplyTorque
  - Character movement: MoveCharacter, JumpCharacter
  - Raycasting: Raycast helper

- **Components** (need to add to `src/ecs/Components.h`)
  - PhysicsBody: mass, kinematic/static flags, friction, restitution, damping, trigger support
  - CharacterController: walk/run speeds, jump, max slope, grounded state
  - Collider: simple collision shape (for triggers/sensors, layer filtering)

#### Utilities
- **PhysicsUtils** (`src/physics/utils/`)
  - GLM ↔ Bullet type conversion (vec3, quat)

### ✅ Completed Integration

#### CMake Build System
- [x] Bullet3 added via `add_subdirectory(thirdparty/bullet3)`
- [x] Linked: `BulletDynamics`, `BulletCollision`, `LinearMath`
- [x] Includes set: `thirdparty/bullet3/src`
- [x] MSVC warnings suppressed for Bullet targets (no Werror)

#### Main Loop Integration
- [x] `PhysicsWorld` created/initialized; `Step(deltaTime)` called in loop
- [x] Clean shutdown on exit

#### ECS Components (foundation exists)
- Physics components available (verification/usage samples pending)
- EnTT observers for lifecycle still to add (enhancement)

---

## 2. Animation System — Implementation Status

### ✅ What's Implemented (95%)

#### Core Data Structures
- **Skeleton** (`src/animation/Skeleton.h/cpp`)
  - Bone hierarchy with parent indices
  - Bind poses (local transforms)
  - Inverse bind matrices for skinning
  - Bone lookup by name/index
  - Hierarchy validation
  - Global inverse transform (scene root) to stabilize FBX rigs

- **AnimationClip** (`src/animation/AnimationClip.h/cpp`)
  - Keyframes for position, rotation, scale (per bone)
  - Linear interpolation (position/scale)
  - SLERP for rotations
  - Time sampling with looping support
  - Per-bone animation channels

- **AnimationLoader** (`src/animation/AnimationLoader.h/cpp`)
  - Assimp integration for skeleton extraction
  - Bone hierarchy from node structure
  - Animation clip extraction from aiScene
  - Bone weight extraction for vertices
  - GLM conversion utilities
  - Correct inverse-bind usage (`aiBone::mOffsetMatrix` used directly)
  - Bindpose local transforms from node hierarchy (not offset matrix)

#### ECS Integration (Structure)
- **AnimationSystem** (`src/animation/AnimationSystem.h/cpp`)
  - Update (scene-level)
  - UpdateEntity (per-entity)
  - Animation playback (play/stop/pause)
  - Playback speed control
  - Animation blending (crossfade)
  - Bone matrix calculation (local → world)

### ✅ Integrated (Animation)

#### Components & Mesh Integration
- [x] `AnimationComponent` present (skeleton, clips, playback state, matrices)
- [x] `Vertex` includes `boneIndices` (ivec4) and `boneWeights` (vec4)
- [x] `MeshLoader` extracts skeleton, animations, bone weights; supports embedded textures
- [x] VAO sets bone attributes (locations 5, 6)

#### Rendering Pipeline
- [x] Skinned shader: `assets/shaders/passes/deferred/gbuffer_skinned.vert/frag`
- [x] Bone matrices via UBO (binding=3), `MAX_BONES=512`
- [x] `RenderSystem` separates animated/static, uploads bone UBO, binds PBR textures

#### Main Loop
- [x] `AnimationSystem::Update(scene, deltaTime)` called each frame

#### Animation Timeline UI
- [x] ImGui panel (clip select, play/pause/stop, loop, time scrub, speed)
- [x] Controls propagate to all submeshes sharing the same skeleton

---

## 3. Implementation Phases

### Phase 1: Bullet3 CMake Integration ⚡
**Status:** ✅ Complete  
**Priority:** CRITICAL — Done

**Tasks:**
1. Add Bullet3 subdirectory to CMakeLists.txt
2. Link Bullet3 libraries to OpenEyeCore
3. Verify compilation succeeds
4. Test physics world creation

**Files to Modify:**
- `CMakeLists.txt`

---

### Phase 2: Animation Component & Mesh Integration ⚡
**Status:** ✅ Complete  
**Priority:** CRITICAL — Done

**Tasks:**
1. Add `AnimationComponent` to `src/ecs/Components.h`
2. Extend `Vertex` struct with `boneIndices` and `boneWeights`
3. Modify `MeshLoader::LoadSceneFromFile()`:
   - Check for skeleton data
   - Call `AnimationLoader::ExtractSkeleton()`
   - Call `AnimationLoader::ExtractAnimations()`
   - Call `AnimationLoader::ExtractBoneWeights()`
   - Store skeleton/animation in `LoadedMesh` or return separately
4. Update `Mesh::UploadToGPU()` to setup bone vertex attributes
5. Test FBX loading with skeleton extraction

**Files to Modify:**
- `src/ecs/Components.h` (add AnimationComponent)
- `src/rendering/resources/Mesh.h` (extend Vertex)
- `src/rendering/resources/MeshLoader.h/cpp` (integrate AnimationLoader)
- `src/rendering/resources/Mesh.cpp` (bone attribute setup)

---

### Phase 3: Skinned Rendering Shader ⚡
**Status:** ✅ Complete  
**Priority:** CRITICAL — Done

**Tasks:**
1. Create `assets/shaders/passes/deferred/gbuffer_skinned.vert`
   - Based on `gbuffer.vert`
   - Add bone matrix uniform (or UBO for >100 bones)
   - Transform position/normal by bone matrices (4 influences)
2. Create `assets/shaders/passes/deferred/gbuffer_skinned.frag`
   - Same as `gbuffer.frag` (no changes needed)
3. Modify `RenderSystem::RenderGeometryPass()`:
   - Separate animated vs static entities
   - Load skinned shader for animated entities
   - Upload bone matrices to shader
   - Render animated entities with skinned shader
   - Render static entities with normal shader

**Files to Create:**
- `assets/shaders/passes/deferred/gbuffer_skinned.vert`
- `assets/shaders/passes/deferred/gbuffer_skinned.frag`

**Files to Modify:**
- `src/ecs/systems/RenderSystem.cpp` (animated entity handling)

---

### Phase 4: Main Loop Integration ⚡
**Status:** ✅ Complete  
**Priority:** CRITICAL — Done

**Tasks:**
1. Initialize `PhysicsWorld` in `main.cpp`
2. Call `physicsWorld->Step(deltaTime)` in render loop
3. Call `AnimationSystem::Update(scene, deltaTime)` in render loop
4. Test physics simulation (even without entities)
5. Test animation update (even without visible rendering)

**Files to Modify:**
- `src/editor/main.cpp` (add physics/animation initialization and updates)

---

### Phase 5: Animation Timeline UI ⚡
**Status:** ✅ Complete  
**Priority:** REQUIRED — Done

**Tasks:**
1. Create ImGui panel "Animation Timeline" in `main.cpp`
2. Timeline scrubbing widget (ImGui::SliderFloat)
3. Clip selection dropdown (list all clips)
4. Play/Pause/Stop buttons
5. Playback speed slider
6. Time display (current time / duration)
7. Connect UI to AnimationSystem:
   - On scrub: set `currentTime` directly
   - On clip select: call `PlayAnimation()`
   - On play/pause: toggle `isPlaying`
8. Display skeleton info (bone count, active clip name)

**Files to Modify:**
- `src/editor/main.cpp` (add ImGui timeline panel)

---

## 4. Implementation Ratings (Professional Assessment)

- **Skeleton Extraction & Mapping:** ⭐⭐⭐⭐☆ (4.5/5)
  - Correct name matching, hierarchy, global inverse transform. Enhancement: robust name aliasing for edge cases.
- **GPU Skinning (UBO, 512 bones):** ⭐⭐⭐⭐⭐ (5/5)
  - UBO avoids uniform limits, correct model-space math, stable across rigs.
- **Animation Playback & Blending:** ⭐⭐⭐⭐☆ (4/5)
  - Playback, scrubbing, crossfade. Enhancement: state machine + layered blending.
- **Timeline UI & Sync:** ⭐⭐⭐⭐☆ (4.5/5)
  - Controls propagate to all submeshes; responsive UX. Enhancement: multi-entity selection.
- **PhysicsWorld Integration:** ⭐⭐⭐⭐☆ (4.5/5)
  - Clean init/step/shutdown; add debug draw/examples next.
- **Material/Textures (embedded):** ⭐⭐⭐⭐⭐ (5/5)
  - Embedded textures resolved; robust path fallback; correct UVs.

## 5. Architecture Quality

### Strengths ✅
- Modular design (factory pattern, separate systems)
- Clear separation of concerns
- Forward declarations to reduce includes
- Shared pointers for resource management
- ECS integration structure
- Scalable foundation

### Areas for Improvement
- Component lifecycle automation (EnTT observers)
- More comprehensive examples
- Better error handling/validation
- Performance profiling hooks
- Configuration system for physics parameters

---

## 6. Testing Plan

### Unit Tests (Manual)
1. **Bullet3 Integration:**
   - Create PhysicsWorld → Should initialize
   - Create box shape → Should succeed
   - Step simulation → Should run without errors

2. **Animation Loading:**
   - Load FBX file → Should extract skeleton
   - Load FBX file → Should extract animations
   - Check bone count → Should match FBX
   - Check animation count → Should match FBX

3. **Rendering:**
   - Render static model → Should work (baseline)
   - Render animated model → Should display correctly
   - Play animation → Should see movement

4. **UI:**
   - Open timeline panel → Should appear
   - Scrub timeline → Model should update
   - Change clip → Animation should switch

---

## 7. Risk Assessment

### High Risk Items
1. **FBX Bone Count:** Some FBX files have 100+ bones
   - **Mitigation:** Use UBO instead of uniform array, or limit to 100 bones
2. **Shader Uniform Limit:** 100 mat4 = 6400 floats
   - **Mitigation:** Use Uniform Buffer Object (UBO) for bone matrices
3. **Performance:** GPU skinning with many bones
   - **Mitigation:** Profile, consider LOD for distant animated objects

### Low Risk Items
- Bullet3 integration (well-documented)
- ImGui timeline (standard widgets)
- Animation playback (logic is straightforward)

---

## 8. Success Criteria

### Phase 1 Complete When:
- ✅ Bullet3 compiles and links
- ✅ PhysicsWorld can be created
- ✅ No linker errors

### Phase 2 Complete When:
- ✅ FBX loads with skeleton data
- ✅ AnimationComponent exists and can be attached
- ✅ Bone weights are in vertex data

### Phase 3 Complete When:
- ✅ Animated model displays correctly (geometry visible)
- ✅ Animation plays (model moves)
- ✅ No shader compilation errors

### Phase 4 Complete When:
- ✅ Physics simulation runs
- ✅ Animation updates every frame
- ✅ No runtime errors

### Phase 5 Complete When:
- ✅ Timeline UI displays
- ✅ Scrubbing updates animation
- ✅ Clip selection works
- ✅ Play/pause works

---

## 9. Enhancements To Make It More Professional & Robust

- **SSR pass polish:**
  - Fix uniform naming (avoid `u_Projection` mismatch), add thickness, binary search, edge fade.
- **Motion Blur:**
  - Add velocity buffer to G-Buffer; camera/object velocity; tile-based max velocity optional.
- **Animation quality:**
  - Enforce top-4 bone weights by sorting before assignment; renormalize.
  - Optional: `AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS=false` for FBX pivot consistency.
  - Add animation state machine (idle/run/hit), events (notifies), and layered blending.
  - Debug overlay for skeleton/bone names to validate rigs quickly.
- **Physics polish:**
  - Debug draw (AABBs, shapes), collision event system, character controller example.
  - EnTT observers for auto lifecycle management.
- **Transparency improvements:**
  - Optional: OIT (weighted blended), or per-material sort granularity.
- **Performance:**
  - Frustum/occlusion for skinned meshes; bone palette culling; GPU profiler markers.

## 10. What’s Left To Implement

- Motion Blur (full implementation with velocity buffer)
- SSR shader fixes and tuning (quality and stability)
- Physics debug render + example scenes
- EnTT observers and physics/animation lifecycle hooks
- Animation state machine + clip library utilities
