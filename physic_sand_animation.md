# Physics and Animation Systems — Implementation Plan & Status

**Last Updated:** 2025-01-XX  
**Overall Status:** Architecture Complete (70%) | Integration Pending (30%)  
**Target:** Display animated FBX model with timeline UI + Bullet3 physics integration

---

## Executive Summary

### Current State
- ✅ **Architecture:** 90% complete - All core classes implemented
- ❌ **Integration:** 20% complete - Missing main loop, CMake, shaders
- ❌ **Rendering:** 0% complete - No skinned shader support
- ❌ **UI:** 0% complete - No animation timeline panel

### Work Required: 7-9 hours
1. **Bullet3 CMake Integration** (30 min) - CRITICAL
2. **Animation Component & Mesh Integration** (2-3 hours) - CRITICAL
3. **Skinned Rendering Shader** (2-3 hours) - CRITICAL
4. **Main Loop Integration** (30 min) - CRITICAL
5. **Animation Timeline UI** (2-3 hours) - REQUIRED FOR TARGET

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

### ❌ Critical Missing Integration

#### CMake Build System (Priority 1 - CRITICAL)
- [ ] **Bullet3 not built:** No `add_subdirectory(thirdparty/bullet3)` in CMakeLists.txt
- [ ] **Not linked:** No Bullet3 libraries linked to OpenEyeCore
- [ ] **Headers accessible:** Code compiles but will fail at link time

**Required Changes:**
```cmake
# Add to CMakeLists.txt after GLFW
add_subdirectory(thirdparty/bullet3)

# Link to OpenEyeCore
target_link_libraries(OpenEyeCore
    PUBLIC BulletDynamics
    PUBLIC BulletCollision
    PUBLIC LinearMath
)

# Add include (if needed)
include_directories(${CMAKE_SOURCE_DIR}/thirdparty/bullet3/src)
```

#### Main Loop Integration (Priority 4 - CRITICAL)
- [ ] **PhysicsWorld not created:** No instance in `main.cpp`
- [ ] **PhysicsSystem::Initialize()** not called
- [ ] **PhysicsSystem::Step()** not called in render loop
- [ ] No physics update happening

**Required Changes:**
```cpp
// In main.cpp initialization
auto physicsWorld = std::make_unique<Physics::PhysicsWorld>();
physicsWorld->Initialize(glm::vec3(0.0f, -9.81f, 0.0f));

// In render loop
physicsWorld->Step(deltaTime);
// PhysicsSystem sync happens automatically or manually
```

#### Component Integration
- [ ] **PhysicsBody component** not in `Components.h`
- [ ] **CharacterController component** not in `Components.h`
- [ ] **Collider component** not in `Components.h`
- [ ] EnTT observers not set up for automatic lifecycle

---

## 2. Animation System — Implementation Status

### ✅ What's Implemented (85%)

#### Core Data Structures
- **Skeleton** (`src/animation/Skeleton.h/cpp`)
  - Bone hierarchy with parent indices
  - Bind poses (local transforms)
  - Inverse bind matrices for skinning
  - Bone lookup by name/index
  - Hierarchy validation

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

#### ECS Integration (Structure)
- **AnimationSystem** (`src/animation/AnimationSystem.h/cpp`)
  - Update (scene-level)
  - UpdateEntity (per-entity)
  - Animation playback (play/stop/pause)
  - Playback speed control
  - Animation blending (crossfade)
  - Bone matrix calculation (local → world)

### ❌ Critical Missing Integration

#### Animation Component (Priority 2 - CRITICAL)
- [ ] **AnimationComponent NOT in `Components.h`**
  - Needs: skeleton pointer, clips array, playback state, bone matrices

**Required Structure:**
```cpp
struct AnimationComponent {
    std::shared_ptr<void> skeleton;  // or std::shared_ptr<Animation::Skeleton>
    std::vector<std::shared_ptr<Animation::AnimationClip>> clips;
    int currentClipIndex = -1;
    float currentTime = 0.0f;
    float playbackSpeed = 1.0f;
    bool isPlaying = false;
    bool loop = true;
    // Blending
    int blendFromClipIndex = -1;
    float blendTime = 0.0f;
    float blendDuration = 0.2f;
    // GPU-ready bone matrices
    std::vector<glm::mat4> finalBoneMatrices;
    bool boneMatricesDirty = true;
};
```

#### Mesh Integration (Priority 2 - CRITICAL)
- [ ] **Vertex struct missing bone data**
  - Current: `position, normal, texCoord, tangent, bitangent`
  - Needs: `glm::ivec4 boneIndices, glm::vec4 boneWeights`

- [ ] **MeshLoader doesn't extract skeleton/animation**
  - FBX loads geometry/textures ✅
  - Skeleton extraction not called ❌
  - Animation extraction not called ❌
  - Bone weights not extracted ❌

**Required Changes to MeshLoader:**
```cpp
// After loading mesh in LoadSceneFromFile:
1. Check if scene has bones (scene->HasMeshes() && mesh->HasBones())
2. Extract skeleton: AnimationLoader::ExtractSkeleton(scene, meshIndex)
3. Extract animations: AnimationLoader::ExtractAnimations(scene, *skeleton)
4. Extract bone weights: AnimationLoader::ExtractBoneWeights(mesh, skeleton, ...)
5. Add bone indices/weights to Vertex struct
6. Return skeleton/animation data with LoadedMesh
```

- [ ] **Mesh VAO doesn't setup bone attributes**
  - Vertex attributes 4, 5 needed for boneIndices, boneWeights

#### Rendering Pipeline (Priority 3 - CRITICAL)
- [ ] **No skinned shader exists**
  - Documented: `assets/shaders/passes/deferred/gbuffer_skinned.vert` ❌ MISSING
  - Current `gbuffer.vert/frag` has no bone matrix support

- [ ] **RenderSystem doesn't handle animated entities**
  - `RenderGeometryPass()` doesn't check for `AnimationComponent`
  - No bone matrix upload to shader
  - No separation of animated vs static entities

**Required Shader (`gbuffer_skinned.vert`):**
```glsl
#version 450
// Same as gbuffer.vert but add:
layout(location = 4) in ivec4 boneIndices;
layout(location = 5) in vec4 boneWeights;
uniform mat4 u_BoneMatrices[MAX_BONES];  // or use UBO

// Transform position/normal by bone matrices
vec4 skinnedPos = boneWeights.x * u_BoneMatrices[boneIndices.x] * vec4(position, 1.0);
// ... (similar for other bone influences)
```

**Required RenderSystem Changes:**
```cpp
// In RenderGeometryPass():
1. Separate entities: animated (has AnimationComponent) vs static
2. For animated: use gbuffer_skinned.vert shader
3. Upload finalBoneMatrices as uniform array or UBO
4. Render animated entities separately
```

#### Main Loop Integration (Priority 4 - CRITICAL)
- [ ] **AnimationSystem::Update()** not called in `main.cpp`
- [ ] No animation update happening

**Required:**
```cpp
// In render loop
Animation::AnimationSystem::Update(scene, deltaTime);
```

#### Animation Timeline UI (Priority 5 - REQUIRED FOR TARGET)
- [ ] **No ImGui timeline panel**
  - Timeline widget (ImGui::SliderFloat for scrubbing)
  - Clip selection dropdown
  - Play/Pause/Stop buttons
  - Playback speed slider
  - Time display (current/duration)

**Required UI Structure:**
```cpp
// In main.cpp ImGui section
ImGui::Begin("Animation Timeline");
// Timeline scrubbing
// Clip selection
// Controls (play/pause/stop/speed)
// Bone count, active clip name display
ImGui::End();
```

---

## 3. Implementation Phases

### Phase 1: Bullet3 CMake Integration ⚡ (30 min) - STARTING NOW
**Status:** Not Started  
**Priority:** CRITICAL - Blocks compilation

**Tasks:**
1. Add Bullet3 subdirectory to CMakeLists.txt
2. Link Bullet3 libraries to OpenEyeCore
3. Verify compilation succeeds
4. Test physics world creation

**Files to Modify:**
- `CMakeLists.txt`

---

### Phase 2: Animation Component & Mesh Integration ⚡ (2-3 hours)
**Status:** Not Started  
**Priority:** CRITICAL - Required for animation

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

### Phase 3: Skinned Rendering Shader ⚡ (2-3 hours)
**Status:** Not Started  
**Priority:** CRITICAL - Required for rendering

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

### Phase 4: Main Loop Integration ⚡ (30 min)
**Status:** Not Started  
**Priority:** CRITICAL - Makes systems functional

**Tasks:**
1. Initialize `PhysicsWorld` in `main.cpp`
2. Call `physicsWorld->Step(deltaTime)` in render loop
3. Call `AnimationSystem::Update(scene, deltaTime)` in render loop
4. Test physics simulation (even without entities)
5. Test animation update (even without visible rendering)

**Files to Modify:**
- `src/editor/main.cpp` (add physics/animation initialization and updates)

---

### Phase 5: Animation Timeline UI ⚡ (2-3 hours)
**Status:** Not Started  
**Priority:** REQUIRED FOR TARGET

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

## 4. Architecture Quality

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

## 5. Testing Plan

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

## 6. Risk Assessment

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

## 7. Success Criteria

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

## 8. Next Steps

**IMMEDIATE:** Start Phase 1 (Bullet3 CMake Integration)

**ORDER:**
1. Phase 1 → Phase 2 → Phase 3 → Phase 4 → Phase 5

**Estimated Total Time:** 7-9 hours of focused development

---

## Notes

- All architecture is production-ready
- Integration is the blocker, not code quality
- Systems are modular and scalable
- Once integrated, adding features (events, compression) is straightforward
