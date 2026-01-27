# Geometry Module - Foundation Implementation

## Overview

This module provides Blender-like mesh operations for AI-driven sculpting and painting. This is Phase 1 (Foundation) implementation.

## Structure

```
eyelib/geometry/
├── core/
│   ├── MeshTopology.h/cpp    - Edge/face structures and adjacency
├── selection/
│   ├── Selection.h/cpp        - Selection system and builders
└── masking/
    └── Mask.h/cpp             - Weight maps and blend modes

api/geometry/
├── Selection.h/cpp            - Public Selection API (PIMPL)
├── Mask.h/cpp                 - Public Mask API (PIMPL)
└── All.h                      - Include all headers
```

## Components

### 1. MeshTopology

**Purpose**: Provides edge and face data structures with adjacency information.

**Features**:
- Edge representation (canonical vertex pairs)
- Face representation (triangles with optional material groups)
- Vertex-to-edge adjacency
- Vertex-to-face adjacency
- Edge-to-face adjacency
- Boundary detection
- Valence queries

**Usage**:
```cpp
MeshTopology topology;
topology.BuildFromIndices(vertexCount, indices);

// Query adjacency
auto edges = topology.GetVertexEdges(vertexID);
auto faces = topology.GetVertexFaces(vertexID);
bool isBoundary = topology.IsBoundaryVertex(vertexID);
```

### 2. Selection

**Purpose**: Immutable selection of vertices, edges, or faces.

**Features**:
- Vertex/edge/face selection
- Selection by IDs, bounds, normal, distance
- Set operations (union, intersect, subtract)
- Fluent builder API

**Usage**:
```cpp
// Using builder (recommended)
auto selection = SelectionBuilder(mesh)
    .ByVertexIDs({0, 1, 2})
    .ByBounds(aabb)
    .ByNormal(Vec3(0, 1, 0), 0.9f)
    .Build();

// Direct creation
auto sel = Selection::FromVertexIDs({0, 1, 2});

// Set operations
auto combined = sel1.Union(sel2);
auto intersection = sel1.Intersect(sel2);
```

### 3. Mask

**Purpose**: Weight maps (0.0 to 1.0) for controlling operation strength.

**Features**:
- Per-vertex weights
- Fill operations
- Paint with falloff
- Blend modes (Replace, Add, Multiply, Subtract, Min, Max)
- Invert

**Usage**:
```cpp
// Create mask
auto mask = Mask::Create(mesh);
mask.Fill(1.0f);  // Fill all

// Paint with falloff
mask.Paint(mesh, selection, 1.0f, 5.0f);  // 5.0 unit falloff

// Blend masks
mask.Blend(otherMask, BlendMode::Multiply);

// Use in operations (coming in Phase 2)
// MeshOps::Smooth(mesh, selection, mask, strength);
```

## API Design Principles

1. **Immutability**: Selections are immutable once created
2. **Fluent API**: Builder pattern for complex selections
3. **PIMPL**: Public API hides implementation details
4. **Type Safety**: Strong typing prevents errors
5. **Deterministic**: All operations are deterministic

## Next Steps (Phase 2)

- Mesh operations (Subdivide, Smooth, Displace, ApplyNoise)
- Primitive generation (Cube, Sphere, Plane, etc.)
- Field system (scalar/vector fields, noise fields)
- Basic unit tests

## Dependencies

- `eyelib::render::Mesh` - Mesh data structure
- `eyelib::math` - Math types (Vec3, AABB, etc.)

## Build Integration

The module is integrated into the main CMake build system:
- `eyelib/geometry/CMakeLists.txt` - Library build
- `api/geometry/CMakeLists.txt` - API build
- Added to main `CMakeLists.txt` after render module

## Status

✅ **Phase 1 Complete**: Foundation (Topology, Selection, Masking)
🚧 **Phase 2 Next**: Mesh Operations
📋 **Phase 3 Planned**: Advanced Operations (Boolean, Remeshing)
