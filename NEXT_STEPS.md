# Next Steps - Development Roadmap

## ✅ Completed Phases

### Phase 0: Bootstrap & Build System - COMPLETE
- ✅ Window system with OpenGL 4.5
- ✅ Logging system
- ✅ Input handling
- ✅ Time management
- ✅ Basic ECS foundation

### Phase 1: Resource & Shader System - COMPLETE
- ✅ Shader manager with hot-reload
- ✅ Texture loader (stb_image)
- ✅ Mesh loader (Assimp) + primitives
- ✅ Material system (PBR properties)
- ✅ Basic rendering pipeline

### Phase 2: ECS & Scene Basics - IN PROGRESS
- ✅ ECS Components (Transform, Renderable, Camera, Light, AABB, Tag)
- ✅ ECS Systems (TransformSystem, RenderSystem, CameraSystem)
- ✅ Hierarchical transforms
- 🚧 Integration with rendering (currently working on)

## 🎯 Immediate Next Steps

### 1. **Complete Phase 2 Integration** (Current Priority)
**Goal**: Render entities through ECS system

Tasks:
- [ ] Update editor to use ECS Scene and RenderSystem
- [ ] Create test scene with multiple entities
- [ ] Verify hierarchical transforms work
- [ ] Test camera component system

**Files to modify**:
- `src/editor/main.cpp` - Use ECS Scene instead of direct rendering
- Create test entities with Renderable components

**Estimated time**: 30 minutes

### 2. **Phase 3: Performance Optimization** (Next)
**Goal**: Handle large scenes efficiently

Tasks:
- [ ] Frustum culling system
- [ ] LOD manager
- [ ] Instance rendering
- [ ] Mesh batching

**Key files to create**:
- `src/rendering/optimization/FrustumCuller.h/cpp`
- `src/rendering/optimization/LODManager.h/cpp`
- `src/rendering/optimization/InstanceRenderer.h/cpp`

**Estimated time**: 2-3 weeks

### 3. **Phase 4: Deferred Rendering** (Future)
**Goal**: Efficient lighting for large scenes

Tasks:
- [ ] G-Buffer creation
- [ ] Deferred lighting pass
- [ ] Shadow mapping
- [ ] Multiple light support

**Key files to create**:
- `src/rendering/passes/GeometryPass.h/cpp`
- `src/rendering/passes/LightingPass.h/cpp`
- `src/rendering/passes/ShadowPass.h/cpp`

**Estimated time**: 2-3 weeks

### 4. **Phase 5: Post-Processing** (Future)
**Goal**: High-quality visual effects

Tasks:
- [ ] Post-processing pipeline
- [ ] HDR tone mapping
- [ ] Bloom effect
- [ ] SSAO, SSR, AA

**Key files to create**:
- `src/rendering/post/PostProcessor.h/cpp`
- `src/rendering/post/ToneMapping.h/cpp`
- `src/rendering/post/Bloom.h/cpp`

**Estimated time**: 1-2 weeks

### 5. **Phase 6: Render-to-Texture** (Future)
**Goal**: Video/browser rendering on quads

Tasks:
- [ ] RenderTarget system
- [ ] Video texture integration (FFmpeg)
- [ ] Quad rendering in 3D space
- [ ] Multiple video screens support

**Key files to create**:
- `src/rendering/rtt/RenderTarget.h/cpp`
- `src/rendering/rtt/VideoTexture.h/cpp`
- `src/rendering/rtt/QuadRenderer.h/cpp`

**Estimated time**: 2-3 weeks

### 6. **Phase 7: Physics Integration** (Future)
**Goal**: Bullet physics integration

Tasks:
- [ ] Bullet world setup
- [ ] Physics components (RigidBody, Collider)
- [ ] Character controller
- [ ] Transform synchronization

**Key files to create**:
- `src/physics/PhysicsWorld.h/cpp`
- `src/physics/PhysicsComponents.h`

**Estimated time**: 1-2 weeks

### 7. **Phase 8: Editor Development Environment** (Future)
**Goal**: Visual world building tools

Tasks:
- [ ] ImGui docking system
- [ ] Scene hierarchy panel
- [ ] Transform editor panel
- [ ] Asset browser
- [ ] Scene save/load (JSON)

**Key files to create**:
- `src/editor/panels/SceneHierarchyPanel.h/cpp`
- `src/editor/panels/InspectorPanel.h/cpp`
- `src/editor/panels/AssetBrowser.h/cpp`

**Estimated time**: 2-3 weeks

### 8. **Phase 9: Lua Scripting** (Future)
**Goal**: Hot-reloadable game logic

Tasks:
- [ ] Lua 5.4 integration
- [ ] Script components
- [ ] C++ API bindings
- [ ] Hot-reload system

**Key files to create**:
- `src/scripting/LuaEngine.h/cpp`
- `src/scripting/Bindings.h/cpp`

**Estimated time**: 2-3 weeks

## 📋 Current Status

**What we have**:
- ✅ Professional project structure
- ✅ Core systems (Window, Input, Time, Log)
- ✅ Complete rendering resource system
- ✅ Camera with WASD/QE movement and mouse look
- ✅ ECS foundation with components and systems

**What's next**:
1. **Right now**: Integrate ECS with editor (Phase 2 completion)
2. **Then**: Performance optimization (Phase 3)
3. **After**: Advanced rendering features

## 🎮 Recommended Development Order

1. **Complete Phase 2** - Get ECS rendering working (1 hour)
2. **Phase 3** - Performance for large scenes (2-3 weeks)
3. **Phase 4** - Deferred rendering for proper lighting (2-3 weeks)
4. **Phase 5** - Post-processing for visual quality (1-2 weeks)
5. **Phase 6** - RTT for video/browser (2-3 weeks)
6. **Phase 7** - Physics for gameplay (1-2 weeks)
7. **Phase 8** - Editor tools for content creation (2-3 weeks)
8. **Phase 9** - Scripting for rapid iteration (2-3 weeks)

## 💡 Key Principles

- **Stay organized** - Keep code clean and professional
- **Test incrementally** - Verify each feature works before moving on
- **Performance first** - Optimize as we go, not at the end
- **Document as we go** - Comments and docs prevent future confusion

---

**Current Focus**: Complete Phase 2 ECS integration, then move to Phase 3 for performance.

