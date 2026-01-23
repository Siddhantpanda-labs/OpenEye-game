# Refactoring Plan: src → eyelib + API Layer

## Overview

This document outlines the comprehensive refactoring plan to transform the current `src/` library structure into a professional engine library named `eyelib/` with a clean, separated API layer. The refactoring follows the architecture defined in `structure.md`.

## Goals

1. **Rename `src/` to `eyelib/`** - Clear library naming
2. **Create top-level `api/` folder** - Client-facing API separated from implementation
3. **Reorganize modules** - Align with industry-grade structure from `structure.md`
4. **Enforce API boundaries** - Ensure OpenGL and internals never leak to clients
5. **Maintain build compatibility** - Update CMakeLists.txt and includes throughout

---

## Phase 1: Preparation & Analysis

### Step 1.1: Document Current State
- [x] Analyze current `src/` structure
- [x] Identify all modules and their dependencies
- [x] Map existing API folders (`ecs/api/`, `rendering/api/`, etc.)
- [x] List all files that include OpenGL/glad headers
- [x] Document CMakeLists.txt structure

### Step 1.2: Create Backup Strategy
- [ ] Create git branch: `refactor/eyelib-restructure`
- [ ] Document current working state
- [ ] Ensure all tests pass before starting

### Step 1.3: Dependency Analysis
- [ ] Map module dependencies (core → math → ecs → render, etc.)
- [ ] Identify circular dependencies
- [ ] List external dependencies (thirdparty)

---

## Phase 2: Directory Structure Creation

### Step 2.1: Create New Directory Structure

Create the following structure (empty folders initially):

```
eyelib/
├── core/
├── math/              # NEW: Extract math from scattered locations
├── ecs/
├── scene/             # NEW: Extract scene management from ecs
├── render/            # Renamed from rendering/
├── physics/
├── animation/
├── audio/             # NEW: Placeholder for future audio system
├── assets/
├── particles/
├── fluids/
├── platform/          # NEW: Extract platform code from core
├── serialize/         # NEW: Extract serialization from ecs
└── debug/             # NEW: Debug utilities

api/
├── core/
├── math/
├── ecs/
├── scene/
├── render/
├── physics/
├── animation/
├── audio/
├── assets/
├── particles/
├── fluids/
└── platform/

include/               # NEW: Internal headers (if needed)
tests/                 # NEW: Unit tests
```

### Step 2.2: Create Module API Folders

For each module in `api/`, create initial structure:
- `All.h` - Convenience header including all module APIs
- Module-specific headers (e.g., `Renderer.h`, `Mesh.h`)

---

## Phase 3: Module-by-Module Refactoring

### Module 1: CORE

**Current Location:** `src/core/`
**Target Location:** `eyelib/core/`
**API Location:** `api/core/`

#### Step 3.1.1: Move Core Implementation
- [ ] Move `src/core/` → `eyelib/core/`
- [ ] Keep structure:
  - `eyelib/core/log/` (Log.cpp, Log.h)
  - `eyelib/core/time/` (Time.cpp, Time.h)
  - `eyelib/core/input/` (Input.cpp, Input.h)
  - `eyelib/core/window/` (Window.cpp, Window.h)
  - `eyelib/core/filesystem/` (ShaderFileWatcher.cpp, ShaderFileWatcher.h)

#### Step 3.1.2: Extract Platform Code
- [ ] Move `eyelib/core/window/` → `eyelib/platform/window/`
- [ ] Move `eyelib/core/input/` → `eyelib/platform/input/`
- [ ] Create `eyelib/platform/filesystem/` for file operations
- [ ] Update includes in moved files

#### Step 3.1.3: Create Core API
- [ ] Create `api/core/Engine.h` - Engine lifecycle (Init, Run, Shutdown)
- [ ] Create `api/core/Time.h` - Time API (DeltaTime, etc.)
- [ ] Create `api/core/Log.h` - Logging API (Info, Warn, Error)
- [ ] Create `api/core/Config.h` - Configuration API
- [ ] Create `api/core/All.h` - Convenience header
- [ ] Ensure API headers use forward declarations, no OpenGL

#### Step 3.1.4: Update Dependencies
- [ ] Update all files that include `core/` headers
- [ ] Update CMakeLists.txt references

---

### Module 2: MATH

**Current Location:** Scattered (glm usage throughout)
**Target Location:** `eyelib/math/`
**API Location:** `api/math/`

#### Step 3.2.1: Create Math Module
- [ ] Create `eyelib/math/` directory
- [ ] Create math types (Vec2, Vec3, Vec4, Mat3, Mat4, Quaternion)
- [ ] Create Transform class
- [ ] Create bounding volume types (AABB, Sphere, Frustum)
- [ ] Create ray/intersection utilities
- [ ] Initially wrap glm, later can replace

#### Step 3.2.2: Create Math API
- [ ] Create `api/math/Vector.h` - Vec2, Vec3, Vec4
- [ ] Create `api/math/Matrix.h` - Mat3, Mat4
- [ ] Create `api/math/Quaternion.h`
- [ ] Create `api/math/Transform.h`
- [ ] Create `api/math/Bounds.h` - AABB, Sphere
- [ ] Create `api/math/Ray.h` - Ray casting
- [ ] Create `api/math/All.h`

#### Step 3.2.3: Replace glm Usage
- [ ] Replace `glm::vec3` → `eyelib::math::Vec3` in implementation
- [ ] Replace `glm::mat4` → `eyelib::math::Mat4` in implementation
- [ ] Keep glm as internal dependency (not exposed in API)
- [ ] Update all includes

---

### Module 3: ECS

**Current Location:** `src/ecs/`
**Target Location:** `eyelib/ecs/`
**API Location:** `api/ecs/`

#### Step 3.3.1: Move ECS Implementation
- [ ] Move `src/ecs/` → `eyelib/ecs/`
- [ ] Keep structure:
  - `eyelib/ecs/systems/` - All system implementations
  - `eyelib/ecs/Components.cpp/h` - Component definitions
  - `eyelib/ecs/Scene.cpp/h` - Scene implementation
  - `eyelib/ecs/entt/` - EnTT dependency

#### Step 3.3.2: Extract Scene Module
- [ ] Move `eyelib/ecs/Scene.cpp/h` → `eyelib/scene/Scene.cpp/h`
- [ ] Move `eyelib/ecs/SceneSerializer.cpp/h` → `eyelib/serialize/SceneSerializer.cpp/h`
- [ ] Create `eyelib/scene/World.h` - World management
- [ ] Update ECS to use scene module

#### Step 3.3.3: Move Existing API
- [ ] Move `eyelib/ecs/api/*` → `api/ecs/*`
- [ ] Review API headers:
  - `api/ecs/Entity.h` - Entity handles
  - `api/ecs/Transform.h` - Transform component API
  - `api/ecs/Components.h` - Component access
  - `api/ecs/Scene.h` - Scene API (high-level)
  - `api/ecs/All.h` - Convenience header
- [ ] Ensure no OpenGL types in API headers
- [ ] Use forward declarations and opaque handles

#### Step 3.3.4: Update ECS Systems
- [ ] Ensure systems don't expose internals
- [ ] Update system includes
- [ ] Verify system execution order is explicit

---

### Module 4: RENDER (renamed from rendering)

**Current Location:** `src/rendering/`
**Target Location:** `eyelib/render/`
**API Location:** `api/render/`

#### Step 3.4.1: Rename and Move
- [ ] Rename `src/rendering/` → `eyelib/render/`
- [ ] Keep structure:
  - `eyelib/render/resources/` - Mesh, Texture, Shader, Material
  - `eyelib/render/passes/` - Render passes
  - `eyelib/render/framegraph/` - Frame graph
  - `eyelib/render/` - Core renderer files

#### Step 3.4.2: Move Existing API
- [ ] Move `eyelib/render/api/*` → `api/render/*`
- [ ] Review and clean API headers:
  - `api/render/Renderer.h` - Renderer interface
  - `api/render/Mesh.h` - Mesh API (no OpenGL types)
  - `api/render/Material.h` - Material API
  - `api/render/Shader.h` - Shader API (no GLSL types)
  - `api/render/Texture.h` - Texture API
  - `api/render/Camera.h` - Camera API
  - `api/render/Light.h` - Light API
  - `api/render/RenderPipeline.h` - Pipeline API
  - `api/render/All.h` - Convenience header

#### Step 3.4.3: Enforce OpenGL Isolation
- [ ] Verify NO OpenGL types in `api/render/*.h`
- [ ] Use opaque handles (e.g., `MeshHandle`, `TextureHandle`)
- [ ] Move all `#include <glad/glad.h>` to `.cpp` files only
- [ ] Ensure API headers use forward declarations

#### Step 3.4.4: Create Render Presets
- [ ] Create `eyelib/render/presets/` folder
- [ ] Add preset renderers:
  - `SpriteRenderer.h/cpp` - 2D sprite rendering
  - `MeshRenderer.h/cpp` - 3D mesh rendering
  - `SkyboxRenderer.h/cpp` - Skybox (already exists, move)
  - `DebugRenderer.h/cpp` - Debug drawing

---

### Module 5: PHYSICS

**Current Location:** `src/physics/`
**Target Location:** `eyelib/physics/`
**API Location:** `api/physics/`

#### Step 3.5.1: Move Physics Implementation
- [ ] Move `src/physics/` → `eyelib/physics/`
- [ ] Keep structure:
  - `eyelib/physics/bodies/` - RigidBodyFactory
  - `eyelib/physics/shapes/` - CollisionShapeFactory
  - `eyelib/physics/controllers/` - CharacterControllerFactory
  - `eyelib/physics/softbody/` - SoftBodyFactory
  - `eyelib/physics/queries/` - PhysicsQuery
  - `eyelib/physics/utils/` - PhysicsUtils

#### Step 3.5.2: Move Existing API
- [ ] Move `eyelib/physics/api/*` → `api/physics/*`
- [ ] Review API headers:
  - `api/physics/PhysicsWorld.h` - World API
  - `api/physics/RigidBody.h` - Rigid body API
  - `api/physics/CollisionShape.h` - Shape API
  - `api/physics/CharacterController.h` - Controller API
  - `api/physics/SoftBody.h` - Soft body API
  - `api/physics/Query.h` - Query API
  - `api/physics/All.h` - Convenience header

#### Step 3.5.3: Create Physics Presets
- [ ] Create `eyelib/physics/presets/` folder
- [ ] Add preset factories:
  - `BoxCollider.h/cpp`
  - `SphereCollider.h/cpp`
  - `StaticGround.h/cpp`
  - `GravityWorld.h/cpp`

---

### Module 6: ANIMATION

**Current Location:** `src/animation/`
**Target Location:** `eyelib/animation/`
**API Location:** `api/animation/`

#### Step 3.6.1: Move Animation Implementation
- [ ] Move `src/animation/` → `eyelib/animation/`
- [ ] Keep structure:
  - `eyelib/animation/controller/` - AnimatorController, BlendTree
  - `eyelib/animation/` - Core animation files

#### Step 3.6.2: Move Existing API
- [ ] Move `eyelib/animation/api/*` → `api/animation/*`
- [ ] Create API headers:
  - `api/animation/AnimationClip.h` - Clip API
  - `api/animation/Animator.h` - Animator API
  - `api/animation/Skeleton.h` - Skeleton API
  - `api/animation/StateMachine.h` - State machine API
  - `api/animation/All.h` - Convenience header

#### Step 3.6.3: Create Animation Presets
- [ ] Create `eyelib/animation/presets/` folder
- [ ] Add preset state machines:
  - `IdleWalkRun.h/cpp` - Common character states
  - `SimpleBlendTree.h/cpp` - Basic blending

---

### Module 7: ASSETS

**Current Location:** `src/assets/`
**Target Location:** `eyelib/assets/`
**API Location:** `api/assets/`

#### Step 3.7.1: Move Assets Implementation
- [ ] Move `src/assets/` → `eyelib/assets/`
- [ ] Keep structure:
  - `eyelib/assets/AssetRegistry.cpp/h`
  - `eyelib/assets/AssetGUID.cpp/h`
  - `eyelib/assets/AssetCooker.cpp/h`

#### Step 3.7.2: Create Assets API
- [ ] Create `api/assets/AssetRegistry.h` - Registry API
  - `Load()`, `Unload()`, `Get()`, `Reload()`
- [ ] Create `api/assets/AssetHandle.h` - Opaque asset handles
- [ ] Create `api/assets/AssetTypes.h` - Asset type enums
- [ ] Create `api/assets/All.h` - Convenience header

#### Step 3.7.3: Create Asset Presets
- [ ] Create `eyelib/assets/presets/` folder
- [ ] Add default assets:
  - `DefaultTexture.h/cpp` - White/checker texture
  - `ErrorShader.h/cpp` - Error shader fallback
  - `PlaceholderMesh.h/cpp` - Default mesh

---

### Module 8: PARTICLES

**Current Location:** `src/particles/`
**Target Location:** `eyelib/particles/`
**API Location:** `api/particles/`

#### Step 3.8.1: Move Particles Implementation
- [ ] Move `src/particles/` → `eyelib/particles/`
- [ ] Keep structure:
  - `eyelib/particles/templates/` - ParticleTemplates
  - `eyelib/particles/ParticleEmitter.cpp/h`
  - `eyelib/particles/ParticleRenderer.cpp/h`

#### Step 3.8.2: Create Particles API
- [ ] Create `api/particles/ParticleEmitter.h` - Emitter API
- [ ] Create `api/particles/ParticleSystem.h` - System API
- [ ] Create `api/particles/ParticlePresets.h` - Preset templates API
- [ ] Create `api/particles/All.h` - Convenience header
- [ ] Ensure no OpenGL types in API

---

### Module 9: FLUIDS

**Current Location:** `src/fluids/`
**Target Location:** `eyelib/fluids/`
**API Location:** `api/fluids/`

#### Step 3.9.1: Move Fluids Implementation
- [ ] Move `src/fluids/` → `eyelib/fluids/`
- [ ] Keep structure:
  - `eyelib/fluids/templates/` - FluidTemplates
  - `eyelib/fluids/FluidSimulation.cpp/h`
  - `eyelib/fluids/FluidRenderer.cpp/h`

#### Step 3.9.2: Create Fluids API
- [ ] Create `api/fluids/FluidSimulation.h` - Simulation API
- [ ] Create `api/fluids/FluidRenderer.h` - Renderer API
- [ ] Create `api/fluids/FluidPresets.h` - Preset templates API
- [ ] Create `api/fluids/All.h` - Convenience header
- [ ] Ensure no OpenGL types in API

---

### Module 10: SCENE (NEW)

**Current Location:** Extracted from `ecs/`
**Target Location:** `eyelib/scene/`
**API Location:** `api/scene/`

#### Step 3.10.1: Create Scene Module
- [ ] Create `eyelib/scene/` directory
- [ ] Move `eyelib/ecs/Scene.cpp/h` → `eyelib/scene/Scene.cpp/h`
- [ ] Create `eyelib/scene/World.h/cpp` - World management
- [ ] Create `eyelib/scene/SceneManager.h/cpp` - Scene loading/unloading

#### Step 3.10.2: Create Scene API
- [ ] Create `api/scene/Scene.h` - Scene API
  - `Load()`, `Unload()`, `GetEntity()`, `CreateEntity()`
- [ ] Create `api/scene/World.h` - World API
- [ ] Create `api/scene/All.h` - Convenience header

---

### Module 11: SERIALIZE (NEW)

**Current Location:** Extracted from `ecs/`
**Target Location:** `eyelib/serialize/`
**API Location:** `api/serialize/`

#### Step 3.11.1: Create Serialize Module
- [ ] Create `eyelib/serialize/` directory
- [ ] Move `eyelib/ecs/SceneSerializer.cpp/h` → `eyelib/serialize/SceneSerializer.cpp/h`
- [ ] Create `eyelib/serialize/ComponentSerializer.h/cpp` - Component serialization
- [ ] Create `eyelib/serialize/WorldSnapshot.h/cpp` - World snapshots

#### Step 3.11.2: Create Serialize API
- [ ] Create `api/serialize/SceneSerializer.h` - Scene save/load
- [ ] Create `api/serialize/ComponentSerializer.h` - Component serialization
- [ ] Create `api/serialize/WorldSnapshot.h` - Snapshot API
- [ ] Create `api/serialize/All.h` - Convenience header

---

### Module 12: PLATFORM (NEW)

**Current Location:** Extracted from `core/`
**Target Location:** `eyelib/platform/`
**API Location:** `api/platform/`

#### Step 3.12.1: Create Platform Module
- [ ] Create `eyelib/platform/` directory
- [ ] Move `eyelib/core/window/` → `eyelib/platform/window/`
- [ ] Move `eyelib/core/input/` → `eyelib/platform/input/`
- [ ] Create `eyelib/platform/filesystem/` - File operations

#### Step 3.12.2: Create Platform API
- [ ] Create `api/platform/Window.h` - Window API
- [ ] Create `api/platform/Input.h` - Input API
- [ ] Create `api/platform/FileSystem.h` - File system API
- [ ] Create `api/platform/All.h` - Convenience header

---

### Module 13: DEBUG (NEW)

**Current Location:** None (scattered)
**Target Location:** `eyelib/debug/`
**API Location:** `api/debug/`

#### Step 3.13.1: Create Debug Module
- [ ] Create `eyelib/debug/` directory
- [ ] Create `eyelib/debug/DebugDraw.h/cpp` - Debug drawing utilities
- [ ] Create `eyelib/debug/Profiler.h/cpp` - Profiling hooks
- [ ] Create `eyelib/debug/Validation.h/cpp` - Validation layers

#### Step 3.13.2: Create Debug API
- [ ] Create `api/debug/DebugDraw.h` - Debug drawing API
- [ ] Create `api/debug/Profiler.h` - Profiling API
- [ ] Create `api/debug/All.h` - Convenience header

---

### Module 14: AUDIO (PLACEHOLDER)

**Current Location:** None
**Target Location:** `eyelib/audio/`
**API Location:** `api/audio/`

#### Step 3.14.1: Create Audio Placeholder
- [ ] Create `eyelib/audio/` directory
- [ ] Create `eyelib/audio/README.md` - Future implementation notes

#### Step 3.14.2: Create Audio API Placeholder
- [ ] Create `api/audio/` directory
- [ ] Create `api/audio/AudioDevice.h` - Placeholder
- [ ] Create `api/audio/Sound.h` - Placeholder
- [ ] Create `api/audio/All.h` - Convenience header

---

## Phase 4: Build System Updates

### Step 4.1: Update Root CMakeLists.txt
- [ ] Update all `src/` references → `eyelib/`
- [ ] Update `add_subdirectory()` calls:
  - `add_subdirectory(eyelib/core)`
  - `add_subdirectory(eyelib/math)`
  - `add_subdirectory(eyelib/ecs)`
  - `add_subdirectory(eyelib/scene)`
  - `add_subdirectory(eyelib/render)`
  - `add_subdirectory(eyelib/physics)`
  - `add_subdirectory(eyelib/animation)`
  - `add_subdirectory(eyelib/assets)`
  - `add_subdirectory(eyelib/particles)`
  - `add_subdirectory(eyelib/fluids)`
  - `add_subdirectory(eyelib/platform)`
  - `add_subdirectory(eyelib/serialize)`
  - `add_subdirectory(eyelib/debug)`
- [ ] Update include directories:
  - `target_include_directories()` for `api/` folder
  - Remove old `src/` include paths

### Step 4.2: Update Module CMakeLists.txt Files
- [ ] Update each module's CMakeLists.txt:
  - Change source paths from `src/module/` → `eyelib/module/`
  - Update include directories
  - Update target names if needed (e.g., `OpenEyeRendering` → `OpenEyeRender`)
- [ ] Ensure API headers are installed/exported correctly

### Step 4.3: Update Application CMakeLists.txt
- [ ] Update `app/editor/CMakeLists.txt`:
  - Change includes from `src/` → `eyelib/` or `api/`
  - Update target links
- [ ] Update `app/openmeta/CMakeLists.txt`:
  - Change includes to use `api/` headers
  - Update target links

---

## Phase 5: Include Path Updates

### Step 5.1: Update All Source Files
- [ ] Find all `#include "src/` references
- [ ] Replace with:
  - `#include "eyelib/` for internal includes
  - `#include "api/` for API includes (in client code)
- [ ] Update relative includes within modules

### Step 5.2: Update API Header Includes
- [ ] Ensure API headers use forward declarations
- [ ] Remove direct includes of implementation files from API headers
- [ ] Use opaque handles instead of concrete types

### Step 5.3: Verify OpenGL Isolation
- [ ] Search for `#include <glad/glad.h>` in API headers
- [ ] Move all OpenGL includes to `.cpp` files only
- [ ] Verify no OpenGL types in `api/` folder

---

## Phase 6: Namespace Updates

### Step 6.1: Establish Namespace Structure
- [ ] Define namespace hierarchy:
  - `eyelib::core` - Core engine
  - `eyelib::math` - Math utilities
  - `eyelib::ecs` - ECS system
  - `eyelib::scene` - Scene management
  - `eyelib::render` - Rendering
  - `eyelib::physics` - Physics
  - `eyelib::animation` - Animation
  - `eyelib::assets` - Assets
  - `eyelib::particles` - Particles
  - `eyelib::fluids` - Fluids
  - `eyelib::platform` - Platform
  - `eyelib::serialize` - Serialization
  - `eyelib::debug` - Debug utilities

### Step 6.2: Update API Namespace
- [ ] API uses same namespace structure
- [ ] Consider `eyelib::api::module` or just `eyelib::module` for API
- [ ] Document namespace conventions

---

## Phase 7: Editor/Game Application Updates

### Step 7.1: Update Editor Application
- [ ] Update `app/editor/` includes:
  - Change from `src/` → `api/` where appropriate
  - Keep `eyelib/` includes for editor-specific internals if needed
- [ ] Update `src/editor/main.cpp` → `eyelib/editor/main.cpp` (if keeping)
- [ ] Verify editor builds and runs

### Step 7.2: Update Game Application
- [ ] Update `app/openmeta/` includes:
  - Use `api/` headers exclusively
  - No direct `eyelib/` includes
- [ ] Verify game builds and runs

### Step 7.3: Update Game Source Structure
- [ ] Ensure `app/openmeta/src/` uses only API
- [ ] Verify no OpenGL includes in game code
- [ ] Test game functionality

---

## Phase 8: Testing & Validation

### Step 8.1: Build Verification
- [ ] Clean build from scratch
- [ ] Verify all modules compile
- [ ] Check for missing includes
- [ ] Fix compilation errors

### Step 8.2: Link Verification
- [ ] Verify all libraries link correctly
- [ ] Check for undefined symbols
- [ ] Fix linking errors

### Step 8.3: Runtime Testing
- [ ] Run editor application
- [ ] Run game application
- [ ] Test core functionality:
  - Scene loading
  - Entity creation
  - Rendering
  - Physics
  - Animation
- [ ] Fix runtime errors

### Step 8.4: API Boundary Verification
- [ ] Verify no OpenGL types in `api/` headers
- [ ] Verify no implementation details leaked
- [ ] Test that clients can use API without including internals

---

## Phase 9: Documentation & Cleanup

### Step 9.1: Update Documentation
- [ ] Update `README.md` with new structure
- [ ] Document API usage examples
- [ ] Update `BUILD.md` if needed
- [ ] Create `API.md` documenting API structure

### Step 9.2: Clean Up
- [ ] Remove old `src/` directory (after verification)
- [ ] Remove temporary files
- [ ] Clean up git history if needed
- [ ] Update `.gitignore` if needed

### Step 9.3: Create Migration Guide
- [ ] Document changes for developers
- [ ] List breaking changes
- [ ] Provide migration examples

---

## Phase 10: Final Verification

### Step 10.1: Code Review Checklist
- [ ] All modules follow structure.md guidelines
- [ ] API headers are clean (no OpenGL, no internals)
- [ ] Implementation uses OpenGL directly (in .cpp files)
- [ ] No circular dependencies
- [ ] Proper namespace usage
- [ ] CMakeLists.txt updated correctly
- [ ] All includes updated

### Step 10.2: Architecture Verification
- [ ] Verify API is a facade (not reimplementation)
- [ ] Verify OpenGL is internal only
- [ ] Verify module boundaries are clear
- [ ] Verify presets are available where needed

### Step 10.3: Performance Check
- [ ] Verify no performance regressions
- [ ] Check build times
- [ ] Profile if needed

---

## Critical Rules (DO NOT BREAK)

1. **API Never Includes OpenGL**
   - No `#include <glad/glad.h>` in `api/` headers
   - No OpenGL types (GLuint, GLenum, etc.) in API headers
   - Use opaque handles instead

2. **API Never Includes Implementation**
   - API headers use forward declarations
   - API headers don't include `eyelib/` implementation headers
   - API is a facade, not a reimplementation

3. **Implementation Can Use OpenGL Freely**
   - OpenGL used directly in `eyelib/` implementation files
   - No backend abstraction needed
   - OpenGL is an internal detail

4. **Module Boundaries**
   - Each module is self-contained
   - Dependencies flow downward (core → math → ecs → render)
   - No circular dependencies

5. **Presets for Common Use Cases**
   - Each module provides ready-to-use presets
   - Presets live in `eyelib/module/presets/`
   - API exposes preset factories

---

## Migration Strategy

### For Existing Code Using `src/`:
1. Replace `#include "src/module/` with `#include "api/module/` for client code
2. Replace `#include "src/module/` with `#include "eyelib/module/` for internal code
3. Update namespace if changed
4. Test thoroughly

### For New Code:
- Always use `api/` headers
- Never include `eyelib/` directly from client code
- Follow namespace conventions

---

## Estimated Timeline

- **Phase 1-2:** 1-2 days (Preparation & Structure)
- **Phase 3:** 5-7 days (Module-by-Module Refactoring)
- **Phase 4-5:** 2-3 days (Build System & Includes)
- **Phase 6-7:** 1-2 days (Namespaces & Applications)
- **Phase 8:** 2-3 days (Testing)
- **Phase 9-10:** 1-2 days (Documentation & Final Checks)

**Total Estimated Time:** 12-19 days

---

## Notes

- This refactoring is **non-breaking** for API consumers if done correctly
- Internal implementation can change freely
- API should remain stable
- Test after each major phase
- Commit after each completed module
- Use feature branch for safety

---

## Success Criteria

✅ All modules moved to `eyelib/`
✅ Top-level `api/` folder created and populated
✅ No OpenGL types in API headers
✅ All builds succeed
✅ All applications run correctly
✅ API is clean and stable
✅ Structure matches `structure.md` requirements
✅ Documentation updated

---

*This plan should be executed methodically, one module at a time, with testing after each major change.*
