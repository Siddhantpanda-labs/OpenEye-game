# Blender-Like Mesh Operations Upgrade Report

**Date**: 2026-01-27  
**Goal**: Upgrade library to support Blender-level mesh sculpting and painting operations as per `Blender_plan.md`

---

## Executive Summary

The library currently has a **solid foundation** for mesh rendering and basic mesh data management, but **lacks all Blender-like mesh operations** required for AI-driven sculpting and painting. This report identifies what exists, what's missing, and what needs to be added.

**Current Status**: ✅ Foundation exists | ❌ Operations missing  
**Gap Assessment**: **~95% of required functionality needs to be implemented**

---

## 1. What Currently Exists ✅

### 1.1 Mesh Data Structure (Foundation - GOOD)

**Location**: `eyelib/render/resources/Mesh.h` & `api/render/Mesh.h`

**What exists:**
- ✅ CPU-side mesh representation (`std::vector<Vertex>`)
- ✅ Vertex attributes: position, normal, uv, uv1, tangent, color, boneIndices, boneWeights
- ✅ Indexed topology (triangles via indices)
- ✅ Submesh support
- ✅ AABB computation
- ✅ GPU upload/download capability
- ✅ Partial vertex updates (`UpdateVertices()` in API)

**Assessment**: **GOOD** - This is the correct foundation. Mesh data lives on CPU, can be modified, and uploaded to GPU.

**Gap**: Missing stable vertex/face IDs for selection, missing custom attribute layers.

---

### 1.2 Basic Mesh Utilities

**What exists:**
- ✅ `GenerateNormals()` - recalculates normals from geometry
- ✅ `GenerateTangents()` - recalculates tangents from UVs
- ✅ `Optimize()` - placeholder vertex cache optimization
- ✅ `MeshOptimizer::GenerateLOD()` - stub (not implemented)

**Assessment**: **BASIC** - Only fundamental utilities exist. No sculpting operations.

---

### 1.3 Primitive Generation (Reference Only)

**Location**: `reference/MeshLoader.cpp` (NOT in library)

**What exists in reference:**
- `CreateCube()`
- `CreateSphere()`
- `CreatePlane()`
- `CreateQuad()`

**Assessment**: **MISSING FROM LIBRARY** - These exist only in reference code, not in `eyelib` or `api`.

---

## 2. What's Missing ❌ (Critical Gaps)

### 2.1 Mesh Operations Module (COMPLETELY MISSING)

**Required Operations** (per `Blender_plan.md`):

#### Topology Operations:
- ❌ `Subdivide(region)` - Subdivision surface / edge subdivision
- ❌ `Extrude(region, vector)` - Extrude faces/edges/vertices
- ❌ `Bevel(edges)` - Bevel/chamfer edges
- ❌ `Boolean(meshA, meshB, operation)` - Union/Difference/Intersection
- ❌ `Decimate(region, target)` - Mesh simplification
- ❌ `Remesh(voxel/quad)` - Voxel or quad remeshing

#### Deformation Operations:
- ❌ `Displace(region, field)` - Apply displacement field
- ❌ `Smooth(region, strength)` - Laplacian smoothing
- ❌ `Inflate/Deflate(region)` - Push/pull vertices along normals
- ❌ `Relax(region)` - Relax mesh topology
- ❌ `ApplyNoise(mesh, params)` - Perlin/FBM noise displacement
- ❌ `LatticeDeform(region)` - Lattice-based deformation

**Impact**: **CRITICAL** - Without these, AI cannot create or modify geometry beyond primitives.

**Priority**: **P0 (Must Have)**

---

### 2.2 Selection & Masking System (COMPLETELY MISSING)

**Required Features** (per `Blender_plan.md`):

#### Selection Primitives:
- ❌ `SelectByVertexID(ids)` - Select by vertex indices
- ❌ `SelectByFaceID(ids)` - Select by face indices
- ❌ `SelectByBounds(bounds)` - Select by bounding volume
- ❌ `SelectByNormal(direction, threshold)` - Select by normal direction
- ❌ `SelectByCurvature(threshold)` - Select by surface curvature
- ❌ `SelectByDistance(point, radius)` - Select by distance from point
- ❌ `SelectByEdgeLoop(edge)` - Select edge loops
- ❌ `SelectByFaceGroup(group)` - Select face groups
- ❌ `SelectByVisionMask(mask)` - Select via vision buffer (hook)

#### Masking System:
- ❌ Weight maps (float per vertex, 0-1)
- ❌ Multiple mask layers
- ❌ Mask blend modes (add, multiply, replace, subtract)
- ❌ Mask painting operations
- ❌ Mask import/export

**Impact**: **CRITICAL** - Sculpting is impossible without selection/masking. Operations must target specific regions.

**Priority**: **P0 (Must Have)**

---

### 2.3 Attribute System (PARTIALLY MISSING)

**What exists:**
- ✅ Basic vertex attributes (position, normal, uv, color, etc.)

**What's missing:**
- ❌ Named attribute layers (custom attributes)
- ❌ Per-vertex custom attributes (float, vec2, vec3, etc.)
- ❌ Face groups / material groups
- ❌ Attribute layer management (add/remove/rename)
- ❌ Attribute transfer between meshes
- ❌ Attribute painting (writing to attributes)

**Impact**: **HIGH** - Limited to fixed vertex format. Cannot add custom data for sculpting/painting.

**Priority**: **P1 (High Priority)**

---

### 2.4 Field System (COMPLETELY MISSING)

**Required Features** (per `Blender_plan.md`):

- ❌ Scalar fields (float per vertex/position)
- ❌ Vector fields (vec3 per vertex/position)
- ❌ Noise fields (Perlin, FBM, curl noise)
- ❌ Distance fields (distance from point/curve/surface)
- ❌ Curve-driven fields
- ❌ Field evaluation (sample field at position)
- ❌ Field operations (add, multiply, blend)

**Impact**: **HIGH** - Fields are essential for procedural sculpting and painting. AI needs mathematical abstractions.

**Priority**: **P1 (High Priority)**

---

### 2.5 Painting System (COMPLETELY MISSING)

**Required Features** (per `Blender_plan.md`):

#### Vertex Color Painting:
- ❌ `PaintVertexColor(region, color, brush)` - Paint vertex colors
- ❌ `FillColor(region, color)` - Fill region with color
- ❌ `GradientColor(region, start, end)` - Apply gradient
- ❌ `NoisePaint(region, params)` - Procedural color painting
- ❌ `BlendColors(region, mode)` - Blend color layers
- ❌ `SampleColor(source, target)` - Transfer colors

#### Texture Painting (v2):
- ❌ UV generation/unwrap
- ❌ Texture-space painting (paint to texture maps)
- ❌ Albedo/normal/roughness map painting
- ❌ Texture projection (image → UV space)
- ❌ Seam blending

**Impact**: **MEDIUM** - Vertex color painting is P0, texture painting can be v2.

**Priority**: **P0 for vertex colors, P2 for texture painting**

---

### 2.6 Primitive Generation (MISSING FROM LIBRARY)

**What exists**: Only in `reference/` folder, not in `eyelib` or `api`

**Required**:
- ❌ `CreateCube()` in library
- ❌ `CreateSphere(segments)` in library
- ❌ `CreatePlane(width, height)` in library
- ❌ `CreateCylinder(radius, height, segments)` in library
- ❌ `CreateCone(radius, height, segments)` in library
- ❌ `CreateTorus(radius, tubeRadius, segments)` in library

**Impact**: **MEDIUM** - Needed as starting points for AI sculpting.

**Priority**: **P1 (High Priority)**

---

### 2.7 Mesh Snapshot/Versioning (MISSING)

**Required Features**:
- ❌ Immutable mesh snapshots (copy-on-write)
- ❌ Version history
- ❌ Undo/redo support
- ❌ Rollback to previous state

**Impact**: **MEDIUM** - Important for AI iteration and safety.

**Priority**: **P2 (Medium Priority)**

---

### 2.8 Edge/Face Topology Access (MISSING)

**Current State**: Only vertex and index arrays exist. No explicit edge/face structures.

**Required**:
- ❌ Edge data structure (vertex pairs)
- ❌ Face data structure (vertex triplets + metadata)
- ❌ Edge/face adjacency queries
- ❌ Edge loops detection
- ❌ Face groups

**Impact**: **HIGH** - Many operations (extrude, bevel, edge loops) require edge/face topology.

**Priority**: **P0 (Must Have)**

---

## 3. Architecture Assessment

### 3.1 Current Architecture (GOOD)

**Strengths:**
- ✅ Clean separation: `eyelib/` (implementation) vs `api/` (public API)
- ✅ PIMPL pattern in API layer
- ✅ CPU-side mesh data (required for editing)
- ✅ Partial update capability (`UpdateVertices`)

**Weaknesses:**
- ❌ No geometry operations module
- ❌ No selection/masking module
- ❌ No field system
- ❌ No painting module

---

### 3.2 Recommended Module Structure

Based on `Blender_plan.md` requirements, add these modules:

```
eyelib/
├── geometry/              ← NEW MODULE
│   ├── MeshOps.h/cpp     (Topology operations)
│   ├── MeshDeform.h/cpp  (Deformation operations)
│   ├── MeshBoolean.h/cpp (Boolean operations)
│   ├── MeshSelect.h/cpp  (Selection system)
│   ├── MeshMask.h/cpp    (Masking system)
│   ├── MeshPaint.h/cpp   (Painting system)
│   ├── Fields.h/cpp      (Field system)
│   └── Primitives.h/cpp  (Primitive generation)

api/
├── geometry/             ← NEW MODULE
│   ├── MeshOps.h/cpp     (Public API)
│   ├── Selection.h/cpp
│   ├── Mask.h/cpp
│   ├── Paint.h/cpp
│   └── Fields.h/cpp
```

---

## 4. Implementation Priority (Phased Approach)

### Phase 1: Foundation (v1 - Critical)

**Goal**: Enable basic sculpting and vertex color painting

1. **Edge/Face Topology** (P0)
   - Add edge/face data structures to Mesh
   - Build adjacency information
   - Edge loop detection

2. **Selection System** (P0)
   - Basic selection (by ID, bounds, normal, distance)
   - Selection storage and queries

3. **Masking System** (P0)
   - Weight maps per vertex
   - Basic blend modes

4. **Basic Mesh Operations** (P0)
   - `Subdivide()` - Simple subdivision
   - `Smooth()` - Laplacian smoothing
   - `Displace()` - Basic displacement
   - `ApplyNoise()` - Perlin noise displacement

5. **Primitive Generation** (P1)
   - Move from reference to library
   - `CreateCube()`, `CreateSphere()`, `CreatePlane()`, etc.

6. **Vertex Color Painting** (P0)
   - Basic paint operations
   - Fill, gradient, noise paint

**Estimated Effort**: 4-6 weeks

---

### Phase 2: Advanced Operations (v2)

1. **Topology Operations**
   - `Extrude()`
   - `Bevel()`
   - `Decimate()`

2. **Boolean Operations**
   - `Union()`, `Difference()`, `Intersection()`

3. **Field System**
   - Scalar/vector fields
   - Noise fields
   - Distance fields

4. **Advanced Deformation**
   - `Inflate/Deflate()`
   - `Relax()`
   - `LatticeDeform()`

**Estimated Effort**: 4-6 weeks

---

### Phase 3: Production Features (v3)

1. **Texture Painting**
   - UV generation/unwrap
   - Texture-space painting
   - Image projection

2. **Remeshing**
   - Voxel remesh
   - Quad remesh

3. **Mesh Snapshot/Versioning**
   - Copy-on-write
   - Undo/redo

**Estimated Effort**: 3-4 weeks

---

## 5. Dependencies & Third-Party Libraries

### Recommended Libraries:

1. **CGAL** (Computational Geometry Algorithms Library)
   - Boolean operations
   - Mesh simplification
   - **License**: GPL/LGPL (check compatibility)

2. **OpenMesh**
   - Half-edge data structure
   - Mesh operations
   - **License**: LGPL

3. **FastNoise2** or **libnoise**
   - Noise generation (Perlin, FBM, etc.)
   - **License**: MIT

4. **Earcut** or **poly2tri**
   - Triangulation for remeshing
   - **License**: MIT

**Alternative**: Implement core algorithms in-house (more control, no license issues, but more work).

---

## 6. API Design Recommendations

### 6.1 Operation-Based API (Not Raw Data Access)

**✅ Good (per Blender_plan.md):**
```cpp
// api/geometry/MeshOps.h
namespace eyelib::api::geometry {
    class MeshOps {
    public:
        // Apply operation to mesh
        static void Subdivide(Mesh& mesh, const Selection& region, int levels);
        static void Extrude(Mesh& mesh, const Selection& region, const Vec3& direction);
        static void Smooth(Mesh& mesh, const Selection& region, float strength);
        static void ApplyNoise(Mesh& mesh, const Selection& region, const NoiseParams& params);
        // ...
    };
}
```

**❌ Bad (exposes internals):**
```cpp
// DON'T DO THIS
mesh.GetVertices()[i].position += offset;  // Direct mutation
```

---

### 6.2 Selection API

```cpp
// api/geometry/Selection.h
namespace eyelib::api::geometry {
    class Selection {
    public:
        static Selection ByVertexIDs(const Mesh& mesh, const std::vector<uint32_t>& ids);
        static Selection ByBounds(const Mesh& mesh, const AABB& bounds);
        static Selection ByNormal(const Mesh& mesh, const Vec3& direction, float threshold);
        static Selection ByDistance(const Mesh& mesh, const Vec3& point, float radius);
        // ...
        
        bool Contains(uint32_t vertexID) const;
        size_t GetVertexCount() const;
        std::vector<uint32_t> GetVertexIDs() const;
    };
}
```

---

### 6.3 Mask API

```cpp
// api/geometry/Mask.h
namespace eyelib::api::geometry {
    class Mask {
    public:
        static Mask Create(const Mesh& mesh);
        void SetWeight(uint32_t vertexID, float weight);
        float GetWeight(uint32_t vertexID) const;
        void Blend(const Mask& other, BlendMode mode);
        void Fill(float value);
        void Paint(const Selection& region, float value, float falloff);
    };
}
```

---

## 7. Critical Design Decisions

### 7.1 Mesh Topology Representation

**Decision Required**: How to represent edges/faces?

**Option A: Half-Edge Structure** (like OpenMesh)
- ✅ Efficient for complex operations
- ✅ Natural edge/face traversal
- ❌ More complex implementation
- ❌ Higher memory overhead

**Option B: Indexed Triangles + Adjacency** (current + adjacency)
- ✅ Simple, matches current structure
- ✅ Lower memory overhead
- ❌ Less efficient for some operations
- ❌ Harder edge loop detection

**Recommendation**: Start with **Option B** (add adjacency to current structure), upgrade to half-edge if needed.

---

### 7.2 Deterministic Operations

**Requirement** (per `Blender_plan.md`): All operations must be deterministic (same input → same output).

**Impact**: 
- No random number generation without seeds
- Fixed-point math where possible
- Deterministic algorithms (no floating-point order dependencies)

---

### 7.3 CPU vs GPU Operations

**Decision**: All mesh operations run on **CPU** (per `Blender_plan.md`).

**Rationale**:
- CPU-side mesh data (already exists)
- Easier to debug and validate
- GPU upload happens after operations complete
- Deterministic execution

---

## 8. Testing Requirements

### 8.1 Unit Tests Needed

- Mesh operation correctness (subdivide, extrude, etc.)
- Selection accuracy
- Mask blending correctness
- Field evaluation accuracy
- Deterministic operation verification

### 8.2 Integration Tests Needed

- Operation chaining (subdivide → smooth → displace)
- Selection + operation combinations
- Mask + operation combinations
- Performance benchmarks

---

## 9. Summary: Gap Analysis

| Category | Status | Priority | Effort |
|----------|--------|----------|--------|
| **Mesh Data Structure** | ✅ Exists | - | - |
| **Topology Operations** | ❌ Missing | P0 | 4-6 weeks |
| **Deformation Operations** | ❌ Missing | P0 | 3-4 weeks |
| **Selection System** | ❌ Missing | P0 | 2-3 weeks |
| **Masking System** | ❌ Missing | P0 | 1-2 weeks |
| **Field System** | ❌ Missing | P1 | 2-3 weeks |
| **Painting System** | ❌ Missing | P0 (vertex), P2 (texture) | 2-3 weeks |
| **Primitive Generation** | ⚠️ Reference only | P1 | 1 week |
| **Boolean Operations** | ❌ Missing | P1 | 2-3 weeks |
| **Edge/Face Topology** | ❌ Missing | P0 | 2-3 weeks |
| **Mesh Snapshot/Versioning** | ❌ Missing | P2 | 1-2 weeks |

**Total Estimated Effort (Phase 1)**: **4-6 weeks**  
**Total Estimated Effort (All Phases)**: **12-18 weeks**

---

## 10. Next Steps

1. **Review this report** - Validate priorities and approach
2. **Design edge/face topology** - Decide on data structure
3. **Implement Phase 1** - Start with selection + basic operations
4. **Create API layer** - Design public API for mesh operations
5. **Add tests** - Unit tests for each operation

---

## 11. References

- `Blender_plan.md` - Requirements specification
- `intro.md` - Architecture principles
- Current mesh implementation: `eyelib/render/resources/Mesh.h`

---

**Report Generated**: 2026-01-27  
**Status**: Ready for implementation planning
