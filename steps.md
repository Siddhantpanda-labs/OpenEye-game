# Production-Grade Engine Library Expansion Plan

## Overview

This document outlines the comprehensive plan to expand `eyelib/` from minimal bring-up to a **full-fledged, powerful, professional, robust, industry-level 2D and 3D game engine library**. Each module will be **detailed, complete, powerful, and generic** to enable creation of games, 3D simulation worlds, or full game engines.

## Goals

1. **Complete Module Implementation** - Each module provides full feature set for production use
2. **Generic & Extensible** - Modules are flexible, reusable, and support custom extensions
3. **High-Level + Low-Level APIs** - Easy-to-use defaults with full control when needed
4. **Industry-Grade Quality** - Performance, robustness, and professional architecture
5. **Clean API Boundaries** - No OpenGL/internal types exposed to clients
6. **Reference src/ Implementations** - Learn from existing patterns, improve structure

---

## Architecture Principles

### API Design
- **High-Level API**: Easy-to-use, opinionated defaults for common cases
- **Low-Level API**: Full control, minimal abstraction overhead
- **No OpenGL Exposure**: All OpenGL types hidden behind handles/opaque types
- **PIMPL Pattern**: Implementation details hidden in `.cpp` files
- **Forward Declarations**: API headers use forward declarations, not includes

### Implementation Design
- **OpenGL Direct Usage**: OpenGL used freely in `eyelib/` implementation
- **No Backend Abstraction**: Direct OpenGL usage, no renderer abstraction layer
- **Data-Oriented**: Cache-friendly structures, SoA where appropriate
- **Multi-Threading Ready**: Thread-safe where needed, job system for parallel work
- **Performance First**: Profiling hooks, optimization opportunities

### Module Organization
- **Self-Contained**: Each module has clear boundaries
- **Dependency Flow**: Core → Math → Platform → Assets → Render/Physics/Animation → ECS/Scene
- **Presets Available**: Common use cases provided out-of-the-box
- **Extensible**: Plugin architecture for custom systems

---

## Phase 1: Math Module Expansion

**Current State**: Basic GLM wrappers (Vec2/3/4, Mat4, Quat, IVec2/3/4)

**Reference**: `src/` uses GLM directly, but we need comprehensive math utilities

### Step 1.1: Geometric Types & Utilities

#### 1.1.1: Bounding Volumes
- [ ] **AABB** (`eyelib/math/AABB.h`)
  - `min`, `max` bounds
  - `GetCenter()`, `GetExtents()`, `GetSize()`
  - `Transform(matrix)` - transform AABB
  - `Contains(point)`, `Intersects(other)`, `Union(other)`
  - `Expand(point)`, `Expand(amount)`
  - `IsValid()` - check for valid bounds

- [ ] **Sphere** (`eyelib/math/Sphere.h`)
  - `center`, `radius`
  - `Contains(point)`, `Intersects(other)`, `Distance(point)`
  - `Transform(matrix)` - approximate transform
  - `Expand(amount)`, `Union(other)`

- [ ] **Capsule** (`eyelib/math/Capsule.h`)
  - `pointA`, `pointB`, `radius`
  - `Contains(point)`, `Distance(point)`
  - `GetCenter()`, `GetHeight()`

- [ ] **Frustum** (`eyelib/math/Frustum.h`)
  - 6 planes (left, right, top, bottom, near, far)
  - `Contains(point)`, `Contains(sphere)`, `Contains(aabb)`
  - `FromMatrix(projection * view)` - extract from matrix
  - `GetCorners()` - 8 corner points

#### 1.1.2: Ray & Intersection
- [ ] **Ray** (`eyelib/math/Ray.h`)
  - `origin`, `direction` (normalized)
  - `GetPoint(t)` - point along ray
  - `Distance(point)` - distance to point
  - `ClosestPoint(point)` - closest point on ray

- [ ] **Plane** (`eyelib/math/Plane.h`)
  - `normal` (normalized), `distance`
  - `Distance(point)` - signed distance
  - `FromPoints(p1, p2, p3)` - construct from triangle
  - `FromNormalPoint(normal, point)`

- [ ] **Intersection Functions** (`eyelib/math/Intersection.h`)
  - `RayPlane(ray, plane)` → intersection point
  - `RaySphere(ray, sphere)` → hit info (t, point, normal)
  - `RayAABB(ray, aabb)` → hit info
  - `RayTriangle(ray, v0, v1, v2)` → hit info (Möller-Trumbore)
  - `SphereSphere(s1, s2)` → intersection
  - `AABBAABB(a1, a2)` → intersection
  - `FrustumSphere(frustum, sphere)` → containment
  - `FrustumAABB(frustum, aabb)` → containment

#### 1.1.3: Transform Utilities
- [ ] **Transform Helpers** (`eyelib/math/TransformUtils.h`)
  - `Compose(position, rotation, scale)` → Mat4
  - `Decompose(matrix)` → position, rotation, scale
  - `LookAt(eye, target, up)` → Mat4
  - `Perspective(fov, aspect, near, far)` → Mat4
  - `Orthographic(left, right, bottom, top, near, far)` → Mat4
  - `Inverse(matrix)` → Mat4
  - `Transpose(matrix)` → Mat4
  - `NormalMatrix(matrix)` → Mat3 (for normal transformation)

#### 1.1.4: Interpolation
- [ ] **Interpolation Functions** (`eyelib/math/Interpolation.h`)
  - `Lerp(a, b, t)` - linear interpolation
  - `Slerp(a, b, t)` - spherical linear interpolation (quaternions)
  - `NLerp(a, b, t)` - normalized lerp (quaternions)
  - `Bezier(p0, p1, p2, p3, t)` - cubic bezier
  - `CatmullRom(p0, p1, p2, p3, t)` - catmull-rom spline
  - `EaseIn(t, power)`, `EaseOut(t, power)`, `EaseInOut(t, power)`

#### 1.1.5: Math API
- [ ] Create `api/math/` wrappers:
  - `api/math/AABB.h`, `api/math/Sphere.h`, `api/math/Capsule.h`
  - `api/math/Ray.h`, `api/math/Plane.h`
  - `api/math/Intersection.h` - intersection functions
  - `api/math/TransformUtils.h` - transform utilities
  - `api/math/Interpolation.h` - interpolation functions
  - `api/math/All.h` - convenience header

**Reference**: `src/rendering/Frustum.h`, `src/ecs/Components.h` (AABB usage)

---

## Phase 2: Core Module Expansion

**Current State**: Log, Time, Engine lifecycle

**Reference**: `src/core/` has log, time, window, input (moved to platform)

### Step 2.1: Memory Management

- [ ] **Allocator System** (`eyelib/core/allocator/`)
  - `LinearAllocator.h/cpp` - stack-like allocator
  - `PoolAllocator.h/cpp` - fixed-size object pool
  - `FreeListAllocator.h/cpp` - general-purpose allocator
  - `Allocator.h` - base allocator interface
  - Thread-safe variants where needed

- [ ] **Memory Utilities** (`eyelib/core/Memory.h`)
  - `AlignUp(ptr, alignment)`, `AlignDown(ptr, alignment)`
  - `IsAligned(ptr, alignment)`
  - `AllocateAligned(size, alignment)`, `FreeAligned(ptr)`
  - `MemoryCopy()`, `MemoryMove()`, `MemorySet()`

### Step 2.2: Job System

- [ ] **Thread Pool** (`eyelib/core/job/ThreadPool.h/cpp`)
  - `ThreadPool` class - manages worker threads
  - `Submit(function)` - submit task
  - `SubmitBatch(functions)` - submit multiple tasks
  - `Wait()` - wait for all tasks
  - `GetThreadCount()` - query thread count

- [ ] **Task Graph** (`eyelib/core/job/TaskGraph.h/cpp`)
  - `TaskGraph` - dependency-based task execution
  - `AddTask(name, function, dependencies)`
  - `Execute()` - execute graph respecting dependencies
  - Parallel execution where possible

- [ ] **Parallel For** (`eyelib/core/job/ParallelFor.h`)
  - `ParallelFor(begin, end, function)` - parallel iteration
  - `ParallelForEach(container, function)` - parallel foreach
  - Chunked execution for cache efficiency

### Step 2.3: Plugin System

- [ ] **Plugin Interface** (`eyelib/core/plugin/Plugin.h`)
  - `IPlugin` interface - base plugin interface
  - `GetName()`, `GetVersion()`, `Initialize()`, `Shutdown()`

- [ ] **Plugin Manager** (`eyelib/core/plugin/PluginManager.h/cpp`)
  - `LoadPlugin(path)` - load dynamic library
  - `UnloadPlugin(name)` - unload plugin
  - `GetPlugin(name)` - get plugin instance
  - `RegisterInterface(name, interface)` - register plugin interfaces

### Step 2.4: Reflection (Optional)

- [ ] **Type Info** (`eyelib/core/reflection/TypeInfo.h`)
  - `TypeInfo` - runtime type information
  - `GetName()`, `GetSize()`, `GetAlignment()`
  - Property access (optional, for editor)

### Step 2.5: Core API Expansion

- [ ] Update `api/core/`:
  - `api/core/Memory.h` - memory utilities API
  - `api/core/Job.h` - job system API
  - `api/core/Plugin.h` - plugin system API (if needed)

**Reference**: Industry patterns (Unity, Unreal job systems)

---

## Phase 3: Platform Module Expansion

**Current State**: Window (GLFW), Input (keyboard/mouse)

**Reference**: `src/core/` has window/input (to be moved)

### Step 3.1: Filesystem

- [ ] **FileSystem** (`eyelib/platform/filesystem/FileSystem.h/cpp`)
  - `ReadFile(path)` → `std::vector<uint8_t>`
  - `WriteFile(path, data)` → bool
  - `FileExists(path)` → bool
  - `GetFileSize(path)` → size_t
  - `GetFileTime(path)` → timestamp
  - `CreateDirectory(path)` → bool
  - `DeleteFile(path)` → bool
  - `ListDirectory(path)` → `std::vector<std::string>`
  - `GetAbsolutePath(relative)` → `std::string`
  - `GetRelativePath(absolute, base)` → `std::string`
  - `NormalizePath(path)` → `std::string`
  - `GetExtension(path)` → `std::string`
  - `GetFilename(path)` → `std::string`
  - `GetDirectory(path)` → `std::string`

### Step 3.2: Threading

- [ ] **Threading Utilities** (`eyelib/platform/threading/`)
  - `Mutex.h/cpp` - mutex wrapper
  - `ConditionVariable.h/cpp` - condition variable
  - `Atomic.h` - atomic operations wrapper
  - `Thread.h/cpp` - thread wrapper
  - `ThreadLocal.h` - thread-local storage

### Step 3.3: Time Expansion

- [ ] **High-Resolution Timer** (`eyelib/platform/time/HighResTimer.h/cpp`)
  - `GetTicks()` → uint64_t (nanoseconds)
  - `TicksToSeconds(ticks)` → double
  - `TicksToMilliseconds(ticks)` → double
  - Cross-platform implementation

- [ ] **Frame Pacing** (`eyelib/platform/time/FramePacer.h/cpp`)
  - `FramePacer` - maintains target FPS
  - `WaitForNextFrame()` - sleep until next frame
  - `GetFrameTime()` - actual frame time
  - `GetTargetFPS()`, `SetTargetFPS(fps)`

### Step 3.4: Platform API Expansion

- [ ] Update `api/platform/`:
  - `api/platform/FileSystem.h` - filesystem API
  - `api/platform/Threading.h` - threading API
  - `api/platform/Time.h` - time utilities API

**Reference**: `src/core/` window/input implementations

---

## Phase 4: Render Module Expansion

**Current State**: Basic Shader, Mesh, Renderer stub

**Reference**: `src/rendering/` has comprehensive rendering system

### Step 4.1: Material System

- [ ] **Material** (`eyelib/render/resources/Material.h/cpp`)
  - Shader assignment (`SetShader()`, `GetShader()`)
  - Texture slots (`SetTexture(name, texture)`, `GetTexture(name)`)
  - PBR properties:
    - `SetAlbedo(color)`, `GetAlbedo()`
    - `SetMetallic(value)`, `GetMetallic()`
    - `SetRoughness(value)`, `GetRoughness()`
    - `SetEmission(color)`, `GetEmission()`
    - `SetAO(value)`, `GetAO()`
    - `SetAlpha(value)`, `GetAlpha()`
  - Convenience texture setters:
    - `SetAlbedoTexture()`, `SetNormalTexture()`
    - `SetMetallicTexture()`, `SetRoughnessTexture()`
    - `SetAOTexture()`, `SetEmissiveTexture()`
    - `SetMetallicRoughnessTexture()` (combined)
  - Custom parameters:
    - `SetFloat(name, value)`, `GetFloat(name)`
    - `SetVec3(name, value)`, `GetVec3(name)`
    - `SetVec4(name, value)`, `GetVec4(name)`
  - Shader keywords (`SetKeyword(name, enabled)`)
  - `Bind()` - bind material for rendering
  - `GetHash()` - for batching (same material = same hash)

- [ ] **Material Instance** (`eyelib/render/resources/MaterialInstance.h/cpp`)
  - Shared material + per-instance overrides
  - `CreateInstance(material)` - create instance
  - Override properties without modifying base material
  - Memory-efficient for many instances

**Reference**: `src/rendering/resources/Material.h` (comprehensive implementation)

### Step 4.2: Texture System

- [ ] **Texture2D** (`eyelib/render/resources/Texture2D.h/cpp`)
  - `Create(width, height, format)` - create empty
  - `LoadFromFile(path)` - load from file
  - `LoadFromMemory(data, size)` - load from memory
  - `SetData(data, x, y, width, height)` - update subregion
  - `GetData()` - readback (optional, slow)
  - `Bind(slot)` - bind to texture unit
  - `GetWidth()`, `GetHeight()`, `GetFormat()`
  - Mipmap generation (`GenerateMipmaps()`)
  - Filtering (`SetFilter(min, mag)`)
  - Wrapping (`SetWrap(u, v)`)

- [ ] **Texture3D** (`eyelib/render/resources/Texture3D.h/cpp`)
  - Similar API to Texture2D
  - `Create(width, height, depth, format)`
  - Volume texture support

- [ ] **TextureCube** (`eyelib/render/resources/TextureCube.h/cpp`)
  - `LoadFromFiles(px, nx, py, ny, pz, nz)` - load 6 faces
  - `LoadFromEquirectangular(path)` - convert HDR equirectangular
  - Cubemap support for skyboxes, IBL

- [ ] **TextureArray** (`eyelib/render/resources/TextureArray.h/cpp`)
  - `Create(width, height, layers, format)`
  - Array texture support (e.g., texture atlases)

- [ ] **Texture Loader** (`eyelib/render/resources/TextureLoader.h/cpp`)
  - `LoadPNG(path)` → Texture2D
  - `LoadJPEG(path)` → Texture2D
  - `LoadHDR(path)` → Texture2D (HDR format)
  - `LoadEXR(path)` → Texture2D (OpenEXR)
  - `LoadDDS(path)` → Texture2D (DDS with mipmaps)
  - `LoadKTX(path)` → Texture2D (KTX format)
  - Async loading support (`LoadAsync(path, callback)`)

- [ ] **Texture Compression** (`eyelib/render/resources/TextureCompression.h`)
  - BC (DXT) compression
  - ASTC compression (if supported)
  - ETC2 compression (if supported)
  - Compression utilities

**Reference**: `src/rendering/resources/Texture.h` (basic implementation)

### Step 4.3: Mesh System

- [ ] **Mesh Expansion** (`eyelib/render/resources/Mesh.h/cpp`)
  - Current: basic vertex/index buffers
  - Add:
    - Multiple vertex attributes (position, normal, tangent, uv, color, bone weights/indices)
    - `SetVertices(data, count, layout)`
    - `SetIndices(data, count, format)` - uint16/uint32
    - `SetSubmesh(index, offset, count)` - submesh support
    - `GetVertexCount()`, `GetIndexCount()`, `GetSubmeshCount()`
    - `GetAABB()` - bounding box
    - `Optimize()` - vertex cache optimization
    - `GenerateNormals()` - compute normals from positions
    - `GenerateTangents()` - compute tangents from normals/UVs

- [ ] **Mesh Loader** (`eyelib/render/resources/MeshLoader.h/cpp`)
  - `LoadGLTF(path)` → `std::vector<Mesh>` (multiple meshes)
  - `LoadFBX(path)` → `std::vector<Mesh>` (via Assimp)
  - `LoadOBJ(path)` → `std::vector<Mesh>` (via Assimp)
  - `LoadDAE(path)` → `std::vector<Mesh>` (via Assimp)
  - Async loading (`LoadAsync(path, callback)`)
  - Material assignment from file
  - Skeleton/animation data extraction

- [ ] **Mesh Optimization** (`eyelib/render/resources/MeshOptimizer.h/cpp`)
  - Vertex cache optimization
  - Overdraw optimization
  - LOD generation (`GenerateLOD(mesh, targetVertexCount)`)
  - Simplification algorithms

- [ ] **Instanced Rendering** (`eyelib/render/InstancedRenderer.h/cpp`)
  - `InstancedRenderer` - render many instances efficiently
  - `SetInstanceData(transforms, count)` - per-instance transforms
  - `SetInstanceData(customData, stride, count)` - custom per-instance data
  - `Render(mesh, material, instanceCount)` - render instances
  - GPU instancing support

- [ ] **Batching** (`eyelib/render/BatchRenderer.h/cpp`)
  - Static batching - combine meshes at load time
  - Dynamic batching - combine meshes at runtime
  - `BatchRenderer` - manages batches
  - `AddMesh(mesh, material, transform)` - add to batch
  - `Flush()` - render all batches
  - Material sorting for efficiency

**Reference**: `src/rendering/resources/Mesh.h`, `src/rendering/resources/MeshLoader.h`, `src/rendering/InstancedRenderer.h`

### Step 4.4: Render Pipeline

- [ ] **Render Passes** (`eyelib/render/passes/`)
  - **GBufferPass** (`GBufferPass.h/cpp`)
    - Render geometry to G-buffer (albedo, normal, metallic-roughness, etc.)
    - `Render(scene, camera)` - render scene to G-buffer
    - `GetGBuffer()` - access G-buffer textures
  - **ShadowPass** (`ShadowPass.h/cpp`)
    - Directional shadow mapping
    - Point light shadows (cubemap)
    - Spot light shadows
    - `Render(scene, light)` - render shadow map
    - `GetShadowMap()` - access shadow map
  - **LightingPass** (`LightingPass.h/cpp`)
    - Deferred lighting
    - PBR lighting calculations
    - Multiple light support
    - `Render(gbuffer, lights, camera)` - compute lighting
  - **TransparencyPass** (`TransparencyPass.h/cpp`)
    - Forward rendering for transparent objects
    - Depth sorting
    - Alpha blending
    - `Render(transparentObjects, camera)` - render transparents

- [ ] **Frame Graph** (`eyelib/render/framegraph/FrameGraph.h/cpp`)
  - `FrameGraph` - declarative render pipeline
  - `AddPass(name, setupFunction)` - add render pass
  - `AddResource(name, description)` - add resource
  - `Compile()` - build execution graph
  - `Execute()` - execute frame
  - Automatic resource management
  - Dependency resolution

- [ ] **Render Pipeline** (`eyelib/render/RenderPipeline.h/cpp`)
  - `RenderPipeline` - orchestrates render passes
  - `AddPass(pass)` - register pass
  - `SetOrder(pass, order)` - set execution order
  - `Render(scene, camera)` - render scene
  - Forward rendering mode
  - Deferred rendering mode
  - Forward+ rendering mode (tiled forward)

**Reference**: `src/rendering/passes/`, `src/rendering/framegraph/FrameGraph.h`, `src/rendering/RenderPipeline.h`

### Step 4.5: Post-Processing

- [ ] **Post-Processor** (`eyelib/render/PostProcessor.h/cpp`)
  - `PostProcessor` - manages post-processing effects
  - `AddEffect(effect)` - add effect
  - `RemoveEffect(effect)` - remove effect
  - `Process(inputTexture, outputTexture)` - apply effects

- [ ] **Post-Processing Effects** (`eyelib/render/effects/`)
  - **Bloom** (`Bloom.h/cpp`)
    - `SetIntensity(value)`, `SetThreshold(value)`
    - `SetKnee(value)` - soft threshold
  - **ToneMapping** (`ToneMapping.h/cpp`)
    - ACES, Reinhard, Uncharted2, etc.
    - `SetMode(mode)`, `SetExposure(value)`
  - **FXAA** (`FXAA.h/cpp`)
    - Fast approximate anti-aliasing
  - **TAA** (`TAA.h/cpp`)
    - Temporal anti-aliasing
    - Jitter, history buffer
  - **SSAO** (`SSAO.h/cpp`)
    - Screen-space ambient occlusion
    - `SetRadius(value)`, `SetIntensity(value)`
  - **Motion Blur** (`MotionBlur.h/cpp`)
    - Velocity-based motion blur
  - **Depth of Field** (`DepthOfField.h/cpp`)
    - Bokeh depth of field
    - `SetFocusDistance(value)`, `SetAperture(value)`

**Reference**: `src/rendering/PostProcessor.h`

### Step 4.6: 2D Rendering

- [ ] **Sprite System** (`eyelib/render/2d/Sprite.h/cpp`)
  - `Sprite` - 2D sprite representation
  - `SetTexture(texture)`, `SetUV(rect)`
  - `SetColor(color)`, `SetPosition(pos)`, `SetRotation(angle)`, `SetScale(scale)`
  - `SetFlip(horizontal, vertical)`

- [ ] **Sprite Renderer** (`eyelib/render/2d/SpriteRenderer.h/cpp`)
  - `SpriteRenderer` - batches and renders sprites
  - `AddSprite(sprite)` - add to batch
  - `Render(camera)` - render all sprites
  - Sprite sheet support
  - Texture atlas support

- [ ] **Text Rendering** (`eyelib/render/2d/TextRenderer.h/cpp`)
  - `Font` - font loading (TTF, OTF)
  - `TextRenderer` - renders text
  - `RenderText(text, font, position, color, size)`
  - Glyph caching
  - Text layout (wrapping, alignment)

- [ ] **2D Primitives** (`eyelib/render/2d/Primitives.h/cpp`)
  - `DrawLine(start, end, color, width)`
  - `DrawCircle(center, radius, color, segments)`
  - `DrawRectangle(min, max, color)`
  - `DrawPolygon(points, color)`

### Step 4.7: Advanced Features

- [ ] **LOD System** (`eyelib/render/LODManager.h/cpp`)
  - `LODManager` - manages level-of-detail
  - `AddLOD(mesh, distance)` - add LOD level
  - `SelectLOD(cameraPosition)` - select appropriate LOD
  - Distance-based LOD
  - Screen-space LOD (optional)

- [ ] **Culling** (`eyelib/render/CullingSystem.h/cpp`)
  - Frustum culling (`CullFrustum(objects, frustum)`)
  - Occlusion culling (optional, advanced)
  - `CullingSystem` - manages culling

- [ ] **IBL** (`eyelib/render/IBL.h/cpp`)
  - Image-based lighting
  - `LoadEnvironmentMap(path)` - load HDR environment
  - `GenerateIBL(environmentMap)` - generate IBL textures
  - Diffuse irradiance map
  - Specular prefiltered map
  - BRDF lookup texture

- [ ] **Render Targets** (`eyelib/render/RenderTarget.h/cpp`)
  - `RenderTarget` - framebuffer wrapper
  - `Create(width, height, attachments)` - create with attachments
  - `AttachTexture(slot, texture)` - attach texture
  - `Bind()` - bind for rendering
  - `Blit(source, destination)` - copy/blit between targets
  - MSAA support

**Reference**: `src/rendering/LODManager.h`, `src/rendering/IBL.h`, `src/rendering/GBuffer.h`, `src/rendering/ShadowMap.h`

### Step 4.8: Render API

- [ ] Update `api/render/`:
  - `api/render/Material.h` - material API (PIMPL)
  - `api/render/Texture.h` - texture API (opaque handles)
  - `api/render/Mesh.h` - mesh API (opaque handles)
  - `api/render/RenderPipeline.h` - pipeline API
  - `api/render/PostProcessor.h` - post-processing API
  - `api/render/Sprite.h` - 2D sprite API
  - `api/render/Text.h` - text rendering API
  - `api/render/Camera.h` - camera API
  - `api/render/Light.h` - light API
  - `api/render/All.h` - convenience header

**Reference**: `src/rendering/api/` (existing API structure)

---

## Phase 5: Physics Module Expansion

**Current State**: Bullet-backed rigid bodies (box only)

**Reference**: `src/physics/` has comprehensive physics system

### Step 5.1: Collision Shapes

- [ ] **Collision Shape Factory** (`eyelib/physics/shapes/CollisionShapeFactory.h/cpp`)
  - `CreateBox(halfExtents)` → `CollisionShapeHandle`
  - `CreateSphere(radius)` → `CollisionShapeHandle`
  - `CreateCapsule(radius, height)` → `CollisionShapeHandle`
  - `CreateCylinder(halfExtents)` → `CollisionShapeHandle`
  - `CreateCone(radius, height)` → `CollisionShapeHandle`
  - `CreateConvexHull(mesh)` → `CollisionShapeHandle`
  - `CreateTriangleMesh(mesh)` → `CollisionShapeHandle` (static)
  - `CreateHeightfield(width, height, data)` → `CollisionShapeHandle`
  - `CreateCompound(shapes, transforms)` → `CollisionShapeHandle`
  - `Destroy(handle)` - destroy shape
  - Shape caching (reuse identical shapes)

**Reference**: `src/physics/shapes/CollisionShapeFactory.h`

### Step 5.2: Rigid Bodies

- [ ] **Rigid Body Factory** (`eyelib/physics/bodies/RigidBodyFactory.h/cpp`)
  - `CreateRigidBody(position, rotation, shape, mass, inertia)` → `RigidBodyHandle`
  - `DestroyRigidBody(handle)` - destroy body
  - `SetTransform(handle, position, rotation)` - set transform
  - `GetTransform(handle)` → `Transform` - get transform
  - `SetVelocity(handle, velocity)` - set linear velocity
  - `GetVelocity(handle)` → `Vec3` - get linear velocity
  - `SetAngularVelocity(handle, velocity)` - set angular velocity
  - `GetAngularVelocity(handle)` → `Vec3` - get angular velocity
  - `ApplyForce(handle, force, point)` - apply force
  - `ApplyImpulse(handle, impulse, point)` - apply impulse
  - `SetMass(handle, mass)` - set mass
  - `GetMass(handle)` → `float` - get mass
  - `SetFriction(handle, friction)` - set friction
  - `SetRestitution(handle, restitution)` - set bounciness
  - `SetLinearDamping(handle, damping)` - set linear damping
  - `SetAngularDamping(handle, damping)` - set angular damping
  - `SetKinematic(handle, kinematic)` - set kinematic mode
  - `IsKinematic(handle)` → `bool` - check if kinematic

**Reference**: `src/physics/bodies/RigidBodyFactory.h`

### Step 5.3: Constraints

- [ ] **Constraint System** (`eyelib/physics/constraints/ConstraintFactory.h/cpp`)
  - `CreateHinge(bodyA, bodyB, pivotA, pivotB, axisA, axisB)` → `ConstraintHandle`
  - `CreateBallSocket(bodyA, bodyB, pivotA, pivotB)` → `ConstraintHandle`
  - `CreateSlider(bodyA, bodyB, frameA, frameB)` → `ConstraintHandle`
  - `CreateFixed(bodyA, bodyB, frameA, frameB)` → `ConstraintHandle`
  - `CreateSpring(bodyA, bodyB, pivotA, pivotB, stiffness, damping)` → `ConstraintHandle`
  - `Create6DOF(bodyA, bodyB, frameA, frameB)` → `ConstraintHandle`
  - `CreateGear(bodyA, bodyB, ratio)` → `ConstraintHandle`
  - `CreateRope(bodyA, bodyB, length)` → `ConstraintHandle`
  - `SetLimits(handle, limits)` - set constraint limits
  - `SetMotor(handle, enabled, targetVelocity, maxForce)` - set motor
  - `DestroyConstraint(handle)` - destroy constraint

### Step 5.4: Character Controller

- [ ] **Character Controller** (`eyelib/physics/controllers/CharacterController.h/cpp`)
  - `CharacterController` - capsule-based character controller
  - `Create(position, radius, height)` → `CharacterControllerHandle`
  - `Move(handle, direction, deltaTime)` - move controller
  - `SetPosition(handle, position)` - set position
  - `GetPosition(handle)` → `Vec3` - get position
  - `SetRotation(handle, rotation)` - set rotation
  - `GetRotation(handle)` → `Quat` - get rotation
  - `IsOnGround(handle)` → `bool` - check if on ground
  - `GetGroundNormal(handle)` → `Vec3` - get ground normal
  - `SetStepHeight(handle, height)` - set step-up height
  - `SetSlopeLimit(handle, angle)` - set max slope angle
  - `SetCollisionFilter(handle, group, mask)` - collision filtering
  - `Destroy(handle)` - destroy controller

**Reference**: `src/physics/controllers/CharacterControllerFactory.h`

### Step 5.5: Physics Queries

- [ ] **Query System** (`eyelib/physics/queries/PhysicsQuery.h/cpp`)
  - `Raycast(from, to)` → `RaycastResult` - single hit
  - `RaycastAll(from, to)` → `std::vector<RaycastResult>` - all hits
  - `RaycastClosest(from, to)` → `RaycastResult` - closest hit
  - `SweepTest(shape, from, to, rotation)` → `SweepResult` - shape sweep
  - `OverlapTest(shape, position, rotation)` → `std::vector<OverlapResult>` - overlap test
  - `QueryAABB(aabb)` → `std::vector<Entity>` - query AABB region
  - `QuerySphere(center, radius)` → `std::vector<Entity>` - query sphere region
  - Result types:
    - `RaycastResult`: `hit`, `point`, `normal`, `distance`, `entity`
    - `SweepResult`: `hit`, `point`, `normal`, `distance`, `entity`, `fraction`
    - `OverlapResult`: `entity`, `contactPoint`, `normal`

**Reference**: `src/physics/queries/PhysicsQuery.h`, `src/physics/queries/PhysicsQueryTypes.h`

### Step 5.6: Advanced Physics

- [ ] **Soft Bodies** (`eyelib/physics/softbody/SoftBodyFactory.h/cpp`)
  - `CreateCloth(width, height, resolution)` → `SoftBodyHandle`
  - `CreateRope(length, segments)` → `SoftBodyHandle`
  - `CreateVolume(mesh)` → `SoftBodyHandle`
  - `SetMass(handle, mass)` - set mass
  - `SetStiffness(handle, stiffness)` - set stiffness
  - `SetDamping(handle, damping)` - set damping
  - `AttachToRigidBody(handle, rigidBody, nodeIndex)` - attach node
  - `GetNodePosition(handle, nodeIndex)` → `Vec3` - get node position
  - `SetNodePosition(handle, nodeIndex, position)` - set node position
  - `ApplyForce(handle, nodeIndex, force)` - apply force to node
  - `Destroy(handle)` - destroy soft body

- [ ] **Cloth Simulation** (`eyelib/physics/softbody/Cloth.h/cpp`)
  - `Cloth` - cloth-specific utilities
  - `SetWind(handle, direction, strength)` - set wind
  - `SetCollision(handle, enabled)` - enable/disable collision
  - `SetSelfCollision(handle, enabled)` - enable/disable self-collision

- [ ] **Vehicle Physics** (`eyelib/physics/vehicle/Vehicle.h/cpp`)
  - `Vehicle` - vehicle physics (optional, advanced)
  - `CreateVehicle(chassis, wheels)` → `VehicleHandle`
  - `SetEnginePower(handle, power)` - set engine power
  - `SetBrakeForce(handle, force)` - set brake force
  - `SetSteeringAngle(handle, angle)` - set steering angle
  - `GetWheelTransform(handle, wheelIndex)` → `Transform` - get wheel transform

**Reference**: `src/physics/softbody/SoftBodyFactory.h`, `src/physics/softbody/templates/ClothTemplates.h`

### Step 5.7: Physics World Expansion

- [ ] **Physics World Expansion** (`eyelib/physics/PhysicsWorld.h/cpp`)
  - Current: basic rigid body creation
  - Add:
    - `SetGravity(gravity)` - set gravity
    - `GetGravity()` → `Vec3` - get gravity
    - `SetSolverIterations(iterations)` - set solver iterations
    - `SetSolverSubSteps(subSteps)` - set sub-steps
    - `SetContactCallback(callback)` - set contact callback
    - `SetCollisionFilter(group, mask)` - collision filtering
    - `GetBodyCount()` → `size_t` - get body count
    - `GetConstraintCount()` → `size_t` - get constraint count
    - Debug drawing (`GetDebugDrawData()` → debug lines)

**Reference**: `src/physics/PhysicsWorld.h` (comprehensive implementation)

### Step 5.8: Physics API

- [ ] Update `api/physics/`:
  - `api/physics/CollisionShape.h` - shape API (opaque handles)
  - `api/physics/RigidBody.h` - rigid body API (opaque handles)
  - `api/physics/Constraint.h` - constraint API (opaque handles)
  - `api/physics/CharacterController.h` - character controller API
  - `api/physics/Query.h` - query API
  - `api/physics/SoftBody.h` - soft body API (opaque handles)
  - `api/physics/All.h` - convenience header

**Reference**: `src/physics/api/PhysicsWorld.h`

---

## Phase 6: Animation Module Expansion

**Current State**: Skeleton, AnimationClip, Animator stub

**Reference**: `src/animation/` has comprehensive animation system

### Step 6.1: Keyframe Interpolation

- [ ] **Keyframe Sampling** (`eyelib/animation/AnimationClip.h/cpp` expansion)
  - Current: basic clip structure
  - Add:
    - `Sample(time, boneIndex)` → `Transform` - sample keyframe
    - `SamplePosition(time, boneIndex)` → `Vec3` - sample position
    - `SampleRotation(time, boneIndex)` → `Quat` - sample rotation
    - `SampleScale(time, boneIndex)` → `Vec3` - sample scale
    - Interpolation modes:
      - Linear interpolation
      - Cubic interpolation (smooth curves)
      - Step interpolation (discrete)
    - `SetInterpolationMode(boneIndex, mode)` - set interpolation mode

- [ ] **Keyframe Editing** (`eyelib/animation/KeyframeEditor.h/cpp`)
  - `AddKeyframe(boneIndex, time, value)` - add keyframe
  - `RemoveKeyframe(boneIndex, time)` - remove keyframe
  - `SetKeyframeValue(boneIndex, time, value)` - modify keyframe
  - `GetKeyframeCount(boneIndex)` → `size_t` - get keyframe count

**Reference**: `src/animation/AnimationClip.h`

### Step 6.2: Animation Blending

- [ ] **Blend System** (`eyelib/animation/BlendSystem.h/cpp`)
  - `Blend(clipA, clipB, weight)` → `Pose` - blend two clips
  - `BlendAdditive(base, additive, weight)` → `Pose` - additive blend
  - `Blend1D(clips, weights)` → `Pose` - 1D blend tree
  - `Blend2D(clips, weights)` → `Pose` - 2D blend tree (e.g., walk/run + direction)

- [ ] **Blend Tree** (`eyelib/animation/BlendTree.h/cpp`)
  - `BlendTree` - node-based blend tree
  - `AddNode(type, clips)` - add blend node
  - `ConnectNodes(from, to)` - connect nodes
  - `Evaluate(parameters)` → `Pose` - evaluate blend tree
  - Node types:
    - Clip node (single clip)
    - Blend node (blend two inputs)
    - Additive node (additive blend)
    - 1D blend node
    - 2D blend node

**Reference**: `src/animation/controller/BlendTree.h`

### Step 6.3: Animation State Machine

- [ ] **State Machine** (`eyelib/animation/AnimationStateMachine.h/cpp`)
  - `AnimationStateMachine` - manages animation states
  - `AddState(name, clip)` → `StateHandle` - add state
  - `AddTransition(from, to, condition)` → `TransitionHandle` - add transition
  - `SetCondition(name, value)` - set condition value
  - `GetCondition(name)` → `bool` - get condition value
  - `SetTrigger(name)` - set trigger (one-shot)
  - `Update(deltaTime)` - update state machine
  - `GetCurrentState()` → `StateHandle` - get current state
  - `GetCurrentPose()` → `Pose` - get current pose

- [ ] **State Machine Conditions** (`eyelib/animation/StateMachineConditions.h`)
  - `Condition` - base condition interface
  - `BoolCondition(name)` - boolean condition
  - `FloatCondition(name, operator, threshold)` - float comparison
  - `TriggerCondition(name)` - trigger condition (one-shot)

- [ ] **State Machine Transitions** (`eyelib/animation/StateMachineTransitions.h`)
  - `Transition` - transition definition
  - `SetDuration(duration)` - transition duration
  - `SetBlendMode(mode)` - blend mode (linear, etc.)
  - `AddCondition(condition)` - add condition

**Reference**: `src/animation/AnimationStateMachine.h`, `src/animation/controller/AnimatorController.h`

### Step 6.4: Animator Expansion

- [ ] **Animator** (`eyelib/animation/Animator.h/cpp` expansion)
  - Current: stub
  - Add:
    - `SetSkeleton(skeleton)` - set skeleton
    - `SetStateMachine(stateMachine)` - set state machine
    - `SetBlendTree(blendTree)` - set blend tree (alternative to state machine)
    - `Update(deltaTime)` - update animator
    - `GetPose()` → `Pose` - get current pose
    - `GetBoneMatrices()` → `std::vector<Mat4>` - get bone matrices
    - `SetParameter(name, value)` - set animator parameter
    - `GetParameter(name)` → `float/bool` - get parameter

- [ ] **Pose** (`eyelib/animation/Pose.h/cpp`)
  - `Pose` - represents bone pose
  - `SetBoneTransform(boneIndex, transform)` - set bone transform
  - `GetBoneTransform(boneIndex)` → `Transform` - get bone transform
  - `GetBoneCount()` → `size_t` - get bone count
  - `Blend(other, weight)` - blend with another pose
  - `ApplyAdditive(additive, weight)` - apply additive pose

**Reference**: `src/animation/controller/AnimatorParams.h`

### Step 6.5: Advanced Animation

- [ ] **Inverse Kinematics** (`eyelib/animation/IK.h/cpp`)
  - `IK` - inverse kinematics solver
  - `SolveCCD(chain, target, iterations)` - CCD solver
  - `SolveFABRIK(chain, target, iterations)` - FABRIK solver
  - `Chain` - IK chain definition
  - `AddBone(boneIndex, length, constraints)` - add bone to chain
  - `SetTarget(target)` - set target position
  - `Solve()` - solve IK

- [ ] **Procedural Animation** (`eyelib/animation/Procedural.h/cpp`)
  - `ProceduralAnimator` - procedural animation hooks
  - `AddBoneModifier(boneIndex, modifier)` - add modifier
  - Modifier types:
    - `SpringModifier` - spring physics
    - `NoiseModifier` - noise-based animation
    - `LookAtModifier` - look-at target
    - `ConstraintModifier` - constraint-based

- [ ] **Animation Events** (`eyelib/animation/AnimationEvents.h/cpp`)
  - `AnimationEvent` - event definition
  - `AddEvent(clip, time, name, data)` - add event
  - `GetEvents(clip, timeStart, timeEnd)` → `std::vector<AnimationEvent>`
  - Event callbacks (`OnEvent(event)`)

### Step 6.6: Animation API

- [ ] Update `api/animation/`:
  - `api/animation/AnimationClip.h` - clip API (already exists, expand)
  - `api/animation/Animator.h` - animator API
  - `api/animation/StateMachine.h` - state machine API
  - `api/animation/BlendTree.h` - blend tree API
  - `api/animation/Skeleton.h` - skeleton API
  - `api/animation/IK.h` - IK API
  - `api/animation/All.h` - convenience header

**Reference**: `src/animation/` comprehensive implementation

---

## Phase 7: ECS Module Expansion

**Current State**: World with Name + Transform only

**Reference**: `src/ecs/` has comprehensive ECS system

### Step 7.1: Generic Component System

- [ ] **Component Registration** (`eyelib/ecs/ComponentRegistry.h/cpp`)
  - `ComponentRegistry` - manages component types
  - `RegisterComponent<T>()` - register component type
  - `GetComponentType<T>()` → `ComponentTypeId` - get type ID
  - `GetComponentSize(typeId)` → `size_t` - get component size
  - `GetComponentAlignment(typeId)` → `size_t` - get alignment

- [ ] **Component Storage** (`eyelib/ecs/ComponentStorage.h/cpp`)
  - `ComponentStorage` - stores components (SoA or AoS)
  - `AddComponent(entity, component)` - add component
  - `RemoveComponent(entity, typeId)` - remove component
  - `GetComponent(entity, typeId)` → `void*` - get component
  - `HasComponent(entity, typeId)` → `bool` - check component
  - `GetComponentCount(typeId)` → `size_t` - get count

### Step 7.2: Component Types

- [ ] **Core Components** (`eyelib/ecs/components/`)
  - **Transform** (already exists, expand):
    - `position`, `rotation`, `scale`
    - `parent` (Entity)
    - `GetLocalMatrix()` → `Mat4`
    - `GetWorldMatrix(world)` → `Mat4`
    - `dirty` flag for caching
  - **Name** (already exists):
    - `name` (std::string)
  - **Renderable** (`Renderable.h`):
    - `mesh` (MeshHandle)
    - `material` (MaterialHandle)
    - `visible` (bool)
    - `lodMeshes` (std::vector<MeshHandle>)
    - `activeLOD` (size_t)
  - **Camera** (`Camera.h`):
    - `camera` (Camera data)
    - `isPrimary` (bool)
    - `fov`, `aspect`, `nearPlane`, `farPlane`
    - `isPerspective` (bool)
  - **Light** (`Light.h`):
    - `type` (Directional, Point, Spot)
    - `color`, `intensity`
    - `range`, `innerConeAngle`, `outerConeAngle`
    - `castShadows` (bool)
  - **RigidBody** (`RigidBody.h`):
    - `bodyHandle` (RigidBodyHandle)
    - `shapeHandle` (CollisionShapeHandle)
    - `syncTransform` (bool) - sync ECS transform to physics
  - **Animator** (`Animator.h`):
    - `animatorHandle` (AnimatorHandle)
    - `skeletonHandle` (SkeletonHandle)
  - **AABB** (`AABB.h`):
    - `min`, `max`
    - `isVisible` (bool) - culling result
    - `GetCenter()`, `GetExtents()`, `GetSize()`
    - `Transform(matrix)` → `AABB`

**Reference**: `src/ecs/Components.h` (comprehensive component definitions)

### Step 7.3: Query System

- [ ] **Query Builder** (`eyelib/ecs/Query.h/cpp`)
  - `Query` - component query
  - `With<T>()` - require component T
  - `Without<T>()` - exclude component T
  - `Has<T>()` - optional component T
  - `Build()` - build query
  - `Execute(world)` → `QueryResult` - execute query

- [ ] **Query Result** (`eyelib/ecs/QueryResult.h`)
  - `QueryResult` - query result iterator
  - `begin()`, `end()` - iterator support
  - `GetEntity()` → `Entity` - get current entity
  - `Get<T>()` → `T&` - get component
  - `GetCount()` → `size_t` - get result count

- [ ] **Query Examples**:
  ```cpp
  // Find all entities with Transform and Renderable
  auto query = Query().With<Transform>().With<Renderable>();
  for (auto result : query.Execute(world)) {
      auto& transform = result.Get<Transform>();
      auto& renderable = result.Get<Renderable>();
      // ...
  }
  ```

### Step 7.4: Systems Framework

- [ ] **System Base** (`eyelib/ecs/System.h`)
  - `ISystem` - base system interface
  - `Update(world, deltaTime)` - update system
  - `GetName()` → `std::string` - get system name
  - `GetDependencies()` → `std::vector<std::string>` - get dependencies
  - `GetUpdateOrder()` → `int` - get update order

- [ ] **System Manager** (`eyelib/ecs/SystemManager.h/cpp`)
  - `SystemManager` - manages systems
  - `RegisterSystem(system, order)` - register system
  - `UpdateAll(world, deltaTime)` - update all systems
  - Dependency resolution
  - Execution order management

- [ ] **System Types** (`eyelib/ecs/systems/`)
  - **TransformSystem** (`TransformSystem.h/cpp`)
    - Updates world matrices
    - Handles parent-child relationships
    - Dirty flag management
  - **RenderSystem** (`RenderSystem.h/cpp`)
    - Renders renderable entities
    - Camera culling
    - Material sorting
  - **PhysicsSystem** (`PhysicsSystem.h/cpp`)
    - Syncs ECS transform to physics
    - Syncs physics transform to ECS
    - Handles physics events
  - **AnimationSystem** (`AnimationSystem.h/cpp`)
    - Updates animators
    - Calculates bone matrices
    - Applies animation to entities
  - **CameraSystem** (`CameraSystem.h/cpp`)
    - Updates camera matrices
    - Finds primary camera
  - **LightSystem** (`LightSystem.h/cpp`)
    - Collects lights for rendering
    - Updates light data
  - **CullingSystem** (`CullingSystem.h/cpp`)
    - Frustum culling
    - Updates AABB visibility flags

**Reference**: `src/ecs/systems/` (comprehensive system implementations)

### Step 7.5: ECS API

- [ ] Update `api/ecs/`:
  - `api/ecs/World.h` - world API (already exists, expand)
  - `api/ecs/Component.h` - component access API
  - `api/ecs/Query.h` - query API
  - `api/ecs/System.h` - system API
  - `api/ecs/Components.h` - component type definitions
  - `api/ecs/All.h` - convenience header

**Reference**: `src/ecs/api/` (existing API structure)

---

## Phase 8: Scene Module Expansion

**Current State**: Hierarchy + world ownership

**Reference**: `src/ecs/Scene.h` (scene management)

### Step 8.1: Prefab System

- [ ] **Prefab** (`eyelib/scene/Prefab.h/cpp`)
  - `Prefab` - entity template
  - `CreatePrefab(name)` → `PrefabHandle` - create prefab
  - `AddComponent(prefab, component)` - add component to prefab
  - `GetComponent(prefab, typeId)` → `void*` - get component
  - `SavePrefab(prefab, path)` - save to file
  - `LoadPrefab(path)` → `PrefabHandle` - load from file

- [ ] **Prefab Instantiation** (`eyelib/scene/PrefabInstantiator.h/cpp`)
  - `Instantiate(scene, prefab, position, rotation)` → `Entity` - instantiate prefab
  - `Instantiate(scene, prefab, parent)` → `Entity` - instantiate as child
  - `DestroyInstance(entity)` - destroy instance
  - Instance tracking (link instances to prefab)

- [ ] **Prefab Variants** (`eyelib/scene/PrefabVariant.h/cpp`)
  - `PrefabVariant` - prefab with overrides
  - `CreateVariant(basePrefab)` → `PrefabVariantHandle`
  - `OverrideComponent(variant, component)` - override component
  - `GetBasePrefab(variant)` → `PrefabHandle` - get base prefab

**Reference**: `src/ecs/Prefab.h`

### Step 8.2: Scene Management

- [ ] **Scene Manager** (`eyelib/scene/SceneManager.h/cpp`)
  - `SceneManager` - manages multiple scenes
  - `CreateScene(name)` → `SceneHandle` - create scene
  - `LoadScene(path)` → `SceneHandle` - load scene from file
  - `UnloadScene(handle)` - unload scene
  - `GetActiveScene()` → `SceneHandle` - get active scene
  - `SetActiveScene(handle)` - set active scene
  - `GetScene(handle)` → `Scene*` - get scene instance

- [ ] **Scene Loading** (`eyelib/scene/SceneLoader.h/cpp`)
  - `LoadSceneAsync(path, callback)` - async scene loading
  - `UnloadSceneAsync(handle, callback)` - async scene unloading
  - Progress tracking
  - Dependency resolution

- [ ] **Scene Hierarchy** (`eyelib/scene/Scene.h/cpp` expansion)
  - Current: basic hierarchy
  - Add:
    - `GetRootEntities()` → `std::vector<Entity>` - get root entities
    - `GetEntityPath(entity)` → `std::string` - get entity path
    - `FindEntity(path)` → `Entity` - find entity by path
    - `GetSiblingCount(entity)` → `size_t` - get sibling count
    - `GetChildCount(entity)` → `size_t` - get child count
    - `MoveEntity(entity, newParent)` - move entity
    - `DuplicateEntity(entity)` → `Entity` - duplicate entity

### Step 8.3: Scene API

- [ ] Update `api/scene/`:
  - `api/scene/Scene.h` - scene API (already exists, expand)
  - `api/scene/Prefab.h` - prefab API
  - `api/scene/SceneManager.h` - scene manager API
  - `api/scene/All.h` - convenience header

**Reference**: `src/ecs/Scene.h`, `src/ecs/Prefab.h`

---

## Phase 9: Assets Module Expansion

**Current State**: AssetId, AssetRegistry (JSON metadata)

**Reference**: `src/assets/` has asset system

### Step 9.1: Asset Loaders

- [ ] **Mesh Loader** (`eyelib/assets/loaders/MeshLoader.h/cpp`)
  - `LoadMesh(path)` → `AssetId` - load mesh
  - `LoadMeshAsync(path, callback)` - async loading
  - Supports: glTF, FBX, OBJ, DAE
  - Material assignment
  - Skeleton/animation extraction

- [ ] **Texture Loader** (`eyelib/assets/loaders/TextureLoader.h/cpp`)
  - `LoadTexture(path)` → `AssetId` - load texture
  - `LoadTextureAsync(path, callback)` - async loading
  - Supports: PNG, JPEG, HDR, EXR, DDS, KTX
  - Mipmap generation
  - Compression

- [ ] **Audio Loader** (`eyelib/assets/loaders/AudioLoader.h/cpp`)
  - `LoadAudio(path)` → `AssetId` - load audio
  - `LoadAudioAsync(path, callback)` - async loading
  - Supports: WAV, OGG, MP3
  - Streaming support

- [ ] **Shader Loader** (`eyelib/assets/loaders/ShaderLoader.h/cpp`)
  - `LoadShader(vertexPath, fragmentPath)` → `AssetId`
  - `LoadShader(computePath)` → `AssetId`
  - GLSL, HLSL, SPIR-V support
  - Shader variant compilation

- [ ] **Animation Loader** (`eyelib/assets/loaders/AnimationLoader.h/cpp`)
  - `LoadAnimation(path)` → `AssetId` - load animation
  - `LoadAnimationAsync(path, callback)` - async loading
  - Supports: glTF, FBX
  - Skeleton extraction

**Reference**: `src/rendering/resources/MeshLoader.h`, `src/animation/AnimationLoader.h`

### Step 9.2: Asset Streaming

- [ ] **Streaming System** (`eyelib/assets/StreamingManager.h/cpp`)
  - `StreamingManager` - manages asset streaming
  - `RequestAsset(path, priority)` - request asset
  - `CancelRequest(path)` - cancel request
  - `GetLoadProgress(path)` → `float` - get load progress
  - `IsLoaded(path)` → `bool` - check if loaded
  - Priority-based loading
  - Memory budget management

- [ ] **Asset Compression** (`eyelib/assets/compression/AssetCompression.h/cpp`)
  - `CompressAsset(assetId, format)` - compress asset
  - `DecompressAsset(assetId)` - decompress asset
  - LZ4, Zstd compression
  - Texture compression (BC, ASTC, ETC2)

### Step 9.3: Asset Registry Expansion

- [ ] **Asset Registry Expansion** (`eyelib/assets/AssetRegistry.h/cpp`)
  - Current: basic metadata
  - Add:
    - `LoadAsset(path, type)` → `AssetId` - load asset
    - `UnloadAsset(assetId)` - unload asset
    - `GetAsset(assetId, type)` → `T*` - get asset (typed)
    - `ReloadAsset(assetId)` - reload asset
    - `GetAssetPath(assetId)` → `std::string` - get asset path
    - `GetAssetType(assetId)` → `AssetType` - get asset type
    - `GetAssetSize(assetId)` → `size_t` - get asset size
    - `GetAssetMemoryUsage()` → `size_t` - get total memory usage
    - Asset dependencies tracking
    - Reference counting

- [ ] **Asset Types** (`eyelib/assets/AssetTypes.h`)
  - `AssetType` enum:
    - `Mesh`, `Texture`, `Material`, `Shader`, `Audio`, `Animation`, `Prefab`, `Scene`
  - Type-safe asset access

**Reference**: `src/assets/AssetRegistry.h`, `src/assets/AssetCooker.h`

### Step 9.4: Assets API

- [ ] Update `api/assets/`:
  - `api/assets/AssetRegistry.h` - registry API (already exists, expand)
  - `api/assets/AssetLoader.h` - loader API
  - `api/assets/AssetTypes.h` - asset type definitions
  - `api/assets/All.h` - convenience header

---

## Phase 10: Serialize Module Expansion

**Current State**: JSON scene serialization

**Reference**: `src/ecs/SceneSerializer.h`

### Step 10.1: Component Serialization

- [ ] **Component Serializer** (`eyelib/serialize/ComponentSerializer.h/cpp`)
  - `SerializeComponent(component, typeId)` → `json` - serialize component
  - `DeserializeComponent(json, typeId)` → `void*` - deserialize component
  - `RegisterSerializer(typeId, serializer)` - register custom serializer
  - Built-in serializers for core components

- [ ] **Serialization Traits** (`eyelib/serialize/SerializationTraits.h`)
  - `IsSerializable<T>` - check if type is serializable
  - `Serialize<T>(value)` → `json` - serialize value
  - `Deserialize<T>(json)` → `T` - deserialize value
  - Support for: primitives, vectors, maps, custom types

### Step 10.2: Binary Serialization

- [ ] **Binary Serializer** (`eyelib/serialize/BinarySerializer.h/cpp`)
  - `BinarySerializer` - binary format serializer
  - `SerializeScene(scene)` → `std::vector<uint8_t>` - serialize to binary
  - `DeserializeScene(data)` → `Scene` - deserialize from binary
  - Versioning support
  - Compression support

- [ ] **World Snapshot** (`eyelib/serialize/WorldSnapshot.h/cpp`)
  - `WorldSnapshot` - world state snapshot
  - `Capture(world)` → `Snapshot` - capture world state
  - `Restore(world, snapshot)` - restore world state
  - Delta snapshots (only changes)
  - Timestamp support

### Step 10.3: Serialize API

- [ ] Update `api/serialize/`:
  - `api/serialize/SceneSerializer.h` - scene serialization API (already exists, expand)
  - `api/serialize/ComponentSerializer.h` - component serialization API
  - `api/serialize/BinarySerializer.h` - binary serialization API
  - `api/serialize/WorldSnapshot.h` - snapshot API
  - `api/serialize/All.h` - convenience header

**Reference**: `src/ecs/SceneSerializer.h`

---

## Phase 11: Particles Module Expansion

**Current State**: CPU emitter system

**Reference**: `src/particles/` has comprehensive particle system

### Step 11.1: Particle System Expansion

- [ ] **Particle System Expansion** (`eyelib/particles/ParticleSystem.h/cpp`)
  - Current: basic emitter
  - Add:
    - Multiple emitters per system
    - Emitter templates
    - Particle forces (gravity, wind, turbulence)
    - Particle collisions
    - Particle sub-emitters
    - Particle trails

- [ ] **Particle Templates** (`eyelib/particles/templates/ParticleTemplates.h/cpp`)
  - `ParticleTemplates` - preset particle configurations
  - `CreateFire()` → `EmitterConfig` - fire preset
  - `CreateSmoke()` → `EmitterConfig` - smoke preset
  - `CreateExplosion()` → `EmitterConfig` - explosion preset
  - `CreateRain()` → `EmitterConfig` - rain preset
  - `CreateSnow()` → `EmitterConfig` - snow preset
  - `CreateSparkles()` → `EmitterConfig` - sparkles preset

- [ ] **Particle Renderer** (`eyelib/particles/ParticleRenderer.h/cpp`)
  - `ParticleRenderer` - renders particles
  - Billboard rendering
  - Mesh-based particles
  - Soft particles (depth-based blending)
  - Particle sorting (back-to-front)

**Reference**: `src/particles/ParticleEmitter.h`, `src/particles/templates/ParticleTemplates.h`, `src/particles/ParticleRenderer.h`

### Step 11.2: Particles API

- [ ] Update `api/particles/`:
  - `api/particles/ParticleSystem.h` - system API (already exists, expand)
  - `api/particles/ParticleTemplates.h` - templates API
  - `api/particles/ParticleRenderer.h` - renderer API
  - `api/particles/All.h` - convenience header

---

## Phase 12: Fluids Module Expansion

**Current State**: CPU grid simulation

**Reference**: `src/fluids/` has comprehensive fluid simulation

### Step 12.1: Fluid Simulation Expansion

- [ ] **Fluid Simulation Expansion** (`eyelib/fluids/FluidSimulation.h/cpp`)
  - Current: basic grid simulation
  - Add:
    - Full Navier-Stokes solver
    - Pressure projection
    - Velocity advection
    - Density advection
    - Buoyancy forces
    - Temperature simulation
    - Obstacle handling
    - Boundary conditions

- [ ] **Fluid Renderer** (`eyelib/fluids/FluidRenderer.h/cpp`)
  - `FluidRenderer` - renders fluid simulation
  - Volume rendering (raymarching)
  - Surface extraction (marching cubes)
  - Particle-based rendering
  - Foam/splash effects

- [ ] **Fluid Templates** (`eyelib/fluids/templates/FluidTemplates.h/cpp`)
  - `FluidTemplates` - preset fluid configurations
  - `CreateSmoke()` → `FluidConfig` - smoke preset
  - `CreateFire()` → `FluidConfig` - fire preset
  - `CreateWater()` → `FluidConfig` - water preset
  - `CreateSteam()` → `FluidConfig` - steam preset

**Reference**: `src/fluids/FluidSimulation.h`, `src/fluids/FluidRenderer.h`, `src/fluids/templates/FluidTemplates.h`

### Step 12.2: Fluids API

- [ ] Update `api/fluids/`:
  - `api/fluids/FluidSimulation.h` - simulation API (already exists, expand)
  - `api/fluids/FluidRenderer.h` - renderer API
  - `api/fluids/FluidTemplates.h` - templates API
  - `api/fluids/All.h` - convenience header

---

## Phase 13: Audio Module Implementation

**Current State**: Not implemented

### Step 13.1: Audio Engine

- [ ] **Audio Device** (`eyelib/audio/AudioDevice.h/cpp`)
  - `AudioDevice` - audio device management
  - `Initialize()` - initialize audio device
  - `Shutdown()` - shutdown audio device
  - `SetMasterVolume(volume)` - set master volume
  - `GetMasterVolume()` → `float` - get master volume
  - OpenAL or custom implementation

- [ ] **Audio Clip** (`eyelib/audio/AudioClip.h/cpp`)
  - `AudioClip` - audio data
  - `LoadFromFile(path)` - load from file
  - `LoadFromMemory(data, size)` - load from memory
  - `GetDuration()` → `float` - get duration
  - `GetSampleRate()` → `int` - get sample rate
  - `GetChannels()` → `int` - get channel count

- [ ] **Audio Source** (`eyelib/audio/AudioSource.h/cpp`)
  - `AudioSource` - audio playback
  - `Play(clip)` - play clip
  - `Stop()` - stop playback
  - `Pause()` - pause playback
  - `SetVolume(volume)` - set volume
  - `SetPitch(pitch)` - set pitch
  - `SetLoop(loop)` - set looping
  - `SetPosition(position)` - set 3D position
  - `SetVelocity(velocity)` - set 3D velocity
  - `IsPlaying()` → `bool` - check if playing
  - `GetTime()` → `float` - get playback time

- [ ] **Audio Listener** (`eyelib/audio/AudioListener.h/cpp`)
  - `AudioListener` - 3D audio listener
  - `SetPosition(position)` - set position
  - `SetOrientation(forward, up)` - set orientation
  - `SetVelocity(velocity)` - set velocity

### Step 13.2: Advanced Audio

- [ ] **Audio Effects** (`eyelib/audio/effects/`)
  - `Reverb` - reverb effect
  - `Echo` - echo effect
  - `Filter` - filter effect
  - `Distortion` - distortion effect

- [ ] **Audio Mixing** (`eyelib/audio/AudioMixer.h/cpp`)
  - `AudioMixer` - audio mixing
  - `CreateGroup(name)` → `AudioGroupHandle` - create group
  - `SetGroupVolume(group, volume)` - set group volume
  - `AddSourceToGroup(source, group)` - add source to group

- [ ] **Audio Events** (`eyelib/audio/AudioEvents.h/cpp`)
  - `AudioEvent` - audio event system
  - `TriggerEvent(name, position)` - trigger event
  - `RegisterEvent(name, clip, parameters)` - register event

### Step 13.3: Audio API

- [ ] Create `api/audio/`:
  - `api/audio/AudioDevice.h` - device API
  - `api/audio/AudioClip.h` - clip API
  - `api/audio/AudioSource.h` - source API
  - `api/audio/AudioListener.h` - listener API
  - `api/audio/AudioMixer.h` - mixer API
  - `api/audio/All.h` - convenience header

---

## Phase 14: Debug Module Expansion

**Current State**: Debug draw, profiler, validation, presets

### Step 14.1: Debug Draw Expansion

- [ ] **Debug Draw Expansion** (`eyelib/debug/DebugDraw.h/cpp`)
  - Current: basic line/AABB/sphere
  - Add:
    - `AddCapsule(center, radius, height, color, duration)`
    - `AddCylinder(center, radius, height, color, duration)`
    - `AddCone(center, radius, height, color, duration)`
    - `AddPlane(center, normal, size, color, duration)`
    - `AddFrustum(frustum, color, duration)`
    - `AddText(position, text, color, duration)`
    - `AddGrid(center, size, divisions, color, duration)`
    - `AddArrow(start, end, color, duration)`
    - `AddAxis(position, rotation, size, duration)`

### Step 14.2: Profiler Expansion

- [ ] **Profiler Expansion** (`eyelib/debug/Profiler.h/cpp`)
  - Current: basic frame timing
  - Add:
    - `BeginSample(name)` / `EndSample()` - manual sampling
    - `GetAverageTime(name)` → `double` - get average time
    - `GetMinTime(name)` → `double` - get min time
    - `GetMaxTime(name)` → `double` - get max time
    - `GetCallCount(name)` → `size_t` - get call count
    - `Reset()` - reset all samples
    - `ExportReport(path)` - export to file
    - GPU profiling (if supported)

### Step 14.3: Debug API

- [ ] Update `api/debug/`:
  - `api/debug/DebugDraw.h` - debug draw API (already exists, expand)
  - `api/debug/Profiler.h` - profiler API (already exists, expand)
  - `api/debug/Presets.h` - presets API (already exists)
  - `api/debug/Validation.h` - validation API
  - `api/debug/All.h` - convenience header

---

## Phase 15: Build System & Integration

### Step 15.1: CMake Updates

- [ ] Update root `CMakeLists.txt`:
  - Add all new modules
  - Update dependencies
  - Add third-party dependencies (Assimp, OpenAL, etc.)
  - Configure build options

- [ ] Update module `CMakeLists.txt`:
  - Add new source files
  - Update include directories
  - Update link libraries

### Step 15.2: Include Path Updates

- [ ] Update all includes:
  - Replace `src/` with `eyelib/` for internal includes
  - Use `api/` for client-facing includes
  - Fix relative includes within modules

### Step 15.3: Namespace Updates

- [ ] Update namespaces:
  - `eyelib::module` for implementation
  - `eyelib::api::module` for API (or just `eyelib::module` if API uses same namespace)

---

## Phase 16: Testing & Validation

### Step 16.1: Build Verification

- [ ] Clean build from scratch
- [ ] Verify all modules compile
- [ ] Check for missing includes
- [ ] Fix compilation errors

### Step 16.2: Link Verification

- [ ] Verify all libraries link correctly
- [ ] Check for undefined symbols
- [ ] Fix linking errors

### Step 16.3: Runtime Testing

- [ ] Test each module functionality
- [ ] Test API boundaries
- [ ] Test performance
- [ ] Fix runtime errors

### Step 16.4: API Boundary Verification

- [ ] Verify no OpenGL types in `api/` headers
- [ ] Verify no implementation details leaked
- [ ] Test that clients can use API without including internals

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
   - Dependencies flow downward (core → math → platform → assets → render/physics/animation → ecs/scene)
   - No circular dependencies

5. **Presets for Common Use Cases**
   - Each module provides ready-to-use presets
   - Presets live in `eyelib/module/presets/` or `eyelib/module/templates/`
   - API exposes preset factories

6. **Generic & Extensible**
   - Modules support custom extensions
   - Plugin architecture where appropriate
   - Template-based where it makes sense

7. **Performance First**
   - Data-oriented design
   - Cache-friendly structures
   - Multi-threading where safe
   - Profiling hooks throughout

---

## Implementation Order

### Priority 1: Foundation (Phases 1-3)
1. Math module expansion (geometric types, utilities)
2. Core module expansion (memory, jobs, plugins)
3. Platform module expansion (filesystem, threading, time)

### Priority 2: Core Systems (Phases 4-6)
4. Render module expansion (materials, textures, meshes, pipeline)
5. Physics module expansion (shapes, constraints, queries)
6. Animation module expansion (interpolation, blending, state machines)

### Priority 3: Integration (Phases 7-9)
7. ECS module expansion (components, queries, systems)
8. Scene module expansion (prefabs, scene management)
9. Assets module expansion (loaders, streaming)

### Priority 4: Advanced Features (Phases 10-13)
10. Serialize module expansion (binary, snapshots)
11. Particles module expansion (templates, renderer)
12. Fluids module expansion (full simulation, renderer)
13. Audio module implementation

### Priority 5: Polish (Phases 14-16)
14. Debug module expansion
15. Build system & integration
16. Testing & validation

---

## Success Criteria

✅ All modules expanded to production-grade
✅ Each module is generic and extensible
✅ High-level and low-level APIs available
✅ No OpenGL types in API headers
✅ All builds succeed
✅ All functionality tested
✅ Performance meets targets
✅ Documentation complete
✅ Structure matches `structure.md` requirements

---

*This plan should be executed methodically, one module at a time, with testing after each major expansion. Reference `src/` implementations for patterns, but improve structure and make it production-grade.*
