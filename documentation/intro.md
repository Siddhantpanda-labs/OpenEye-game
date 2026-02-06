# OpenEye Engine & Simulation World - Project Overview

## 🎯 Project Intent

**OpenEye** is a **3D engine library** (`eyelib`) with a **client-side simulation world** designed for **AI-first control**. The ultimate vision is a multiplayer VR metaverse where:

- **Players** explore a large urban world, play shooting games, solve puzzles, and interact socially
- **AI** deeply controls, modifies, and creates gameplay systems, assets, and animations in real-time
- **Admin** guides AI through natural language, while AI autonomously executes world operations
- **Future**: AI acts as a movie director, creating sets, animating scenes, and recording content

This is **not** a traditional game engine. It's a **programmable, inspectable, modifiable universe** where AI operates as a first-class agent alongside human players.

---

## 🏗️ Architecture Overview

### Two-Layer Structure

```
┌─────────────────────────────────────────┐
│   Client: Simulation World (WCA)       │  ← AI-controlled, player-facing
│   - World Kernel                        │
│   - Game Systems                        │
│   - VR/Media Integration                │
└──────────────┬──────────────────────────┘
               │ Uses
┌──────────────┴──────────────────────────┐
│   Library: eyelib (Engine Core)        │  ← Stable, tested, API-only
│   - Rendering (OpenGL)                  │
│   - Physics (Bullet)                    │
│   - Animation                           │
│   - Audio (OpenAL)                      │
│   - Scripting (Lua)                     │
│   - ECS                                 │
└─────────────────────────────────────────┘
```

### Core Principle: **API Abstraction**

- **`eyelib/`**: Implementation library (internal, can change)
- **`api/`**: Public API layer (stable, PIMPL pattern)
- **`app/`**: Client applications (world kernel, test apps)

**Rule**: Client code (`app/`) **never** includes `eyelib/` headers directly. Everything goes through `api/`.

---

## 📋 What's Been Accomplished

### ✅ Engine Library (`eyelib`)

**Rendering System**
- Forward & deferred rendering pipelines
- PBR material system (albedo, normal, metallic, roughness)
- IBL (Image-Based Lighting) with irradiance & prefilter maps
- Skybox support (FBX & glTF formats)
- Render targets for off-screen rendering
- Shader system with hot-reload capability

**Animation System**
- Unity-style canonical skeleton format (all models normalized)
- Skinned mesh rendering with bone palette
- Animation clip playback
- Procedural animation support
- Rigging utilities (bone editing, weight calculation)

**Model Loading**
- FBX & glTF support via Assimp
- Automatic material extraction
- Static & animated model support
- Unified pipeline (no shader heuristics)

**Physics**
- Bullet Physics integration
- Rigid body dynamics
- Collision detection

**Audio**
- OpenAL integration
- 3D spatial audio
- Master volume control

**Scripting**
- Lua integration
- Script component system
- Property injection (C++ → Lua)
- Entity userdata with validation
- Hot-reload support

**ECS (Entity Component System)**
- Component registry
- System scheduling
- Scene hierarchy
- Prefab system

**Math & Utilities**
- Vector/matrix math library
- AABB collision
- Asset registry
- Serialization support
- Debug drawing

### ✅ API Layer (`api/`)

**Complete API Coverage**
- All engine features exposed through clean API
- PIMPL pattern for ABI stability
- No direct library dependencies in client code
- Namespace: `eyelib::api::*`

**Key API Modules**
- `api/render/` - Rendering, materials, shaders, meshes
- `api/animation/` - Animation clips, skeletons, rigging
- `api/physics/` - Physics world, rigid bodies
- `api/audio/` - Audio device, sources, listeners
- `api/scripting/` - Script loading, VM management
- `api/scene/` - Scene management, prefabs
- `api/assets/` - Model loading, asset management

### ✅ Testing & Validation

**Test Suite (`app/test/`)**
- Comprehensive API tests (32+ test cases)
- All modules validated
- Test scene with:
  - Skybox rendering
  - Static & animated models
  - PBR materials
  - Camera controls (WASD + mouse)
  - Animation playback toggle

**Quality Assurance**
- All tests passing
- No memory leaks
- Proper resource cleanup
- Cross-platform compatibility (Windows focus)

---

## 🎯 Design Principles & Rules

### 1. **API-First Architecture**

**Rule**: Client code (`app/`) **must never** include `eyelib/` headers.

- ✅ **Correct**: `#include "api/render/Mesh.h"`
- ❌ **Wrong**: `#include "eyelib/render/Mesh.h"`

**Why**: Maintains abstraction, enables future refactoring, ensures ABI stability.

### 2. **Unity-Style Model Pipeline**

**Principle**: All models normalize to canonical skinned format.

- **Static models**: Single-bone skeleton with identity bind pose
- **Animated models**: Full skeleton with animation clips
- **Rendering**: Single skinned shader path (no branching)
- **Result**: Unified pipeline, no heuristics, predictable behavior

**Key Invariant**: 
```
All spatial meaning flows through bones.
```

### 3. **AI-First World Design**

**Principle**: World Kernel is designed for AI control, not human convenience.

**Requirements**:
- **Deterministic**: Fixed update order, reproducible results
- **Hot-editable**: Runtime modification without restart
- **Introspectable**: AI can query world state
- **Serializable**: Snapshots, replay, branching
- **Headless-capable**: Logic runs without rendering

**World Representation**: Semantic graph (not just ECS)
- Nodes: Entity, Component, System, Rule, Script, Asset, Region
- Relations: Entity→Component, System→Component, Rule→System

### 4. **Separation of Concerns**

**Layer 1: World Kernel** (untouchable)
- ECS storage
- Deterministic simulation loop
- Physics stepping
- Event system
- Serialization

**Layer 2: World Logic** (AI playground)
- Rule graphs
- Behavior graphs
- Game systems
- Templates

**Layer 3: Presentation** (human-facing)
- Rendering
- Audio
- VR input
- UI

### 5. **Scripting System Rules**

**Property Assignment**:
- Properties stored in `ScriptComponent.properties` (C++ side)
- Automatically injected into Lua script table on initialization
- Supports: `Bool`, `Number`, `String`, `Vec3`, `Entity`
- Entities passed as validated userdata (not raw integers)

**Script Structure**:
```lua
MyScript = {
    speed = 5.0,  -- Default (can be overridden by C++)
    
    OnStart = function(self, entity)
        -- Properties injected before this runs
    end,
    
    OnUpdate = function(self, deltaTime)
        -- Game logic
    end
}
```

---

## ⚠️ Precautions & Guidelines

### 1. **Never Break API Compatibility**

**Rule**: Once an API is exposed, maintain backward compatibility.

- Use PIMPL pattern to hide implementation changes
- Add new functions, don't remove old ones
- Version APIs if breaking changes are necessary

### 2. **Library Implementation Can Change**

**Rule**: `eyelib/` internals can be refactored freely, as long as `api/` remains stable.

- ✅ Refactor `eyelib/render/RenderPipeline.cpp`
- ❌ Change `api/render/Mesh.h` public interface without versioning

### 3. **No Direct Library Dependencies in Client**

**Rule**: Client code (`app/`) must only use `api/` headers.

**Enforcement**: 
- CMake checks (if implemented)
- Code review requirement
- Linter warnings

### 4. **Model Loading: Format Considerations**

**Current State**:
- ✅ FBX: Full support (static & animated)
- ✅ glTF: Supported (may have orientation/texture quirks)
- ⚠️ glTF static models: May require format-specific handling

**Precaution**: Test models in both formats. Prefer FBX for production if glTF issues persist.

### 5. **Animation System: Canonical Format**

**Rule**: All models must normalize to skinned format.

- Static models get default skeleton (root bone, identity weights)
- Animated models use full skeleton from file
- Rendering always uses skinned shader (identity bones for static = no deformation)

**Precaution**: Don't bypass normalization. Don't use shader heuristics.

### 6. **World Kernel: Determinism is Critical**

**Rule**: World Kernel must be deterministic for AI control.

**Requirements**:
- Fixed timestep physics
- Deterministic system order
- No random state in simulation logic
- Deterministic serialization

**Precaution**: Test determinism with replay. Any non-determinism breaks AI reasoning.

### 7. **Scripting: Property Injection Timing**

**Rule**: Properties are injected **before** `OnStart()` is called.

**Precaution**: Scripts should not assume properties exist before `OnStart()`. Default values in Lua are overridden by C++ properties.

### 8. **Entity Userdata: Always Validate**

**Rule**: Entities passed to Lua must be validated.

- Use `EntityUserdata` wrapper (not raw integers)
- Check `world->IsAlive(entity)` before operations
- Return `NullEntity` for invalid entities

**Precaution**: Never trust entity IDs from Lua without validation.

### 9. **Resource Management: RAII Pattern**

**Rule**: All resources must be properly managed.

- Use smart pointers (`std::shared_ptr`, `std::unique_ptr`)
- RAII for OpenGL resources
- Proper cleanup in destructors
- No raw `new`/`delete` in client code

### 10. **Testing: API Coverage**

**Rule**: Every API feature must have a test.

- New API functions → new test cases
- Test both success and failure paths
- Test edge cases (null pointers, invalid handles, etc.)

---

## 🚀 Current Status

### ✅ **Completed**

- Engine library (`eyelib`) - Fully functional
- API layer (`api/`) - Complete coverage
- Model loading - FBX & glTF support
- Animation system - Unity-style canonical format
- Rendering pipeline - Forward & deferred, PBR
- Scripting system - Lua integration with property injection
- Test suite - Comprehensive validation

### 🚧 **In Progress**

- World Kernel design & implementation
- AI integration layer planning
- VR integration planning
- **Render thread unified scene** ([Render_Thread_Unified_Scene_Plan.md](plans/Render_Thread_Unified_Scene_Plan.md)): Phases 0–5 complete; robust presentation (single framebuffer size, zero-size skip, valid-output guard, vsync) in place. Next: Phase 6 (asset/shaders on render thread).

### 📋 **Planned**

- Multiplayer networking
- VR support
- Media integration (video screens)
- AI world control system
- Progressive asset creation
- Movie director mode

---

## 🎓 Key Learnings & Decisions

### **Why Unity-Style Model Pipeline?**

**Problem**: Static vs animated models required different shader paths (heuristic).

**Solution**: Normalize all models to skinned format. Static models get identity skeleton.

**Result**: Single shader path, no branching, predictable behavior, easier to extend.

### **Why API Abstraction?**

**Problem**: Direct library usage makes refactoring difficult.

**Solution**: PIMPL pattern, stable API layer, implementation hidden.

**Result**: Can refactor library internals without breaking client code.

### **Why AI-First Design?**

**Problem**: Traditional game engines are human-centric, hard for AI to reason about.

**Solution**: World as semantic graph, introspection API, manipulation API, deterministic.

**Result**: AI can understand, query, and modify world state naturally.

---

## 📚 Documentation Structure

- **`intro.md`** (this file) - Project overview
- **`metaverse.md`** - Original vision & planning
- **`AI_Sim.md`** - AI integration architecture
- **`World_kernel_Ai.md`** - World Kernel specification
- **`steps.md`** - Development roadmap
- **`advise.md`** - Architecture guidance & best practices
- **`plans/Render_Thread_Unified_Scene_Plan.md`** - Render thread + unified scene (Phases 0–5 done; next Phase 6)

---

## 🔗 External Dependencies

- **Assimp** - Model loading (FBX, glTF, etc.)
- **Bullet Physics** - Physics simulation
- **OpenAL** - 3D audio
- **Lua** - Scripting language
- **OpenGL** - Graphics rendering
- **GLFW** - Window management
- **GLM** - Math utilities (optional, may be replaced)

---

## 🎯 Success Criteria

### **Engine Library**
- ✅ All features exposed through stable API
- ✅ No direct library dependencies in client code
- ✅ Comprehensive test coverage
- ✅ Industry-grade architecture

### **World Kernel** (Future)
- ✅ Deterministic simulation
- ✅ AI introspection & manipulation APIs
- ✅ Region-scoped game systems
- ✅ Hot-editing support
- ✅ Serialization & replay

### **AI Integration** (Future)
- ✅ AI can query world state
- ✅ AI can create/modify entities
- ✅ AI can build game systems
- ✅ AI can create assets progressively
- ✅ AI can direct "movies"

---

## 📝 Notes for Contributors

1. **Always use API layer** - Never include `eyelib/` headers in client code
2. **Follow Unity-style model pipeline** - Normalize all models to skinned format
3. **Maintain API stability** - Use PIMPL, version APIs if breaking changes needed
4. **Test everything** - Add tests for new API features
5. **Document decisions** - Update this file when architecture changes
6. **AI-first thinking** - Design for AI control, human convenience second

---

**Last Updated**: 2026-02-05  
**Status**: Engine library complete; Render Thread plan Phases 0–5 + robust presentation done (Render_Thread_Unified_Scene_Plan.md). World Kernel in design phase.
