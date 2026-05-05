Good. Let’s **wrap everything cleanly and architect it properly**.
I’ll give you a **complete AAA-grade asset module design** tailored to **your engine, your canonical formats, and your Option-A model**.

This will be **structured like a real engine design document** so you can implement it directly.

---

# 1. Design Goals

Your asset system must achieve:

### Deterministic identity

Assets must have **stable GUIDs across machines and git clones**.

### Canonical runtime formats

Runtime never loads FBX/GLB.

Only:

```text
.omesh
.mat
.png/.jpg
.anim
.oea (package)
```

### Dependency awareness

```text
Scene
 └ Mesh (.omesh)
      └ Material (.mat)
            └ Texture (.png)
```

### Runtime performance

Requirements:

* O(1) asset lookup
* fast startup
* hot reload
* dependency graph
* streaming capability

---

# 2. Final Architecture Overview

Complete system:

```text
Importer Layer
     ↓
Canonical Assets
     ↓
Meta System (.meta)
     ↓
Asset Database
     ↓
Runtime Asset Manager
     ↓
GPU resources
```

Inside the engine:

```text
eyelib/
 ├ render
 ├ ecs
 ├ physics
 ├ world
 └ assets   ← new module
```

**Current location:** The asset pipeline is implemented under **`EngineSource/eyelib/asset_pipe/`** (core, meta, database, runtime, readers, writers, tools/importer). The legacy **`eyelib/assets`** module is deprecated; see **`documentation/plans/asset_migration.md`** for steps to remove it and use asset_pipe everywhere.

During development you build it in:

```text
asset_pipe/
```

Then move it to:

```text
eyelib/assets
```

---

# 3. Canonical Asset Rules

One file = one asset.

```text
car.omesh        → Mesh asset
car.mat          → Material asset
albedo.png       → Texture asset
walk.anim        → Animation asset
city.oea         → Package asset
```

Submeshes inside `.omesh` are **not assets**.

This was your chosen **Option A**.

---

# 4. GUID System

### Type

128-bit GUID.

```cpp
struct Guid
{
    uint64_t high;
    uint64_t low;
};
```

String representation:

```
9f4c2b3e8c1249e1a3d9c45c2c6a7e11
```

### Required operations

```cpp
Guid Generate();
Guid FromString(string);
string ToString();

bool operator==(Guid);
```

Hash:

```cpp
struct GuidHash
{
    size_t operator()(const Guid& g) const;
};
```

---

# 5. Meta File System

Every asset file has a `.meta`.

Example:

```text
car.omesh
car.omesh.meta
```

Meta example:

```yaml
guid: 9f4c2b3e8c1249e1a3d9c45c2c6a7e11
type: Mesh

dependencies:
  - 12fa91bcff22
  - 887ac1dd9233

importer: model
version: 1
```

Material meta:

```yaml
guid: 12fa91bcff22
type: Material

dependencies:
  - a91c23ee9901
```

Texture meta:

```yaml
guid: a91c23ee9901
type: Texture
```

---

# 6. Asset Database

The **central brain**.

Memory structure:

```cpp
struct AssetInfo
{
    Guid guid;

    AssetType type;

    std::filesystem::path path;

    std::vector<Guid> dependencies;
};
```

Container:

```cpp
unordered_map<Guid, AssetInfo> assets;
```

Reverse lookup:

```cpp
unordered_map<string, Guid> pathToGuid;
```

---

# 7. Asset Database Cache

Disk scanning is slow.

So cache registry:

```text
Library/
   asset_database.bin
```

Startup algorithm:

```
1 load cache
2 scan Assets/
3 detect changes
4 update registry
```

Cache contents:

```
GUID
path
type
dependencies
timestamp
```

---

# 8. Asset Manager (Runtime Loader)

Responsible for loading assets.

Interface:

```cpp
class AssetManager
{
public:

    Mesh* LoadMesh(Guid);

    Material* LoadMaterial(Guid);

    Texture* LoadTexture(Guid);

    Animation* LoadAnimation(Guid);
};
```

Runtime flow:

```
scene
  ↓
GUID
  ↓
AssetDatabase
  ↓
path
  ↓
reader
  ↓
runtime asset
```

---

# 9. Runtime Asset Cache

Avoid duplicate loads.

Cache:

```cpp
unordered_map<Guid, shared_ptr<Mesh>> meshes;
unordered_map<Guid, shared_ptr<Material>> materials;
unordered_map<Guid, shared_ptr<Texture>> textures;
```

Load algorithm:

```
if asset cached → return
else → read file → cache → return
```

---

# 10. Dependency Graph

Stored in database.

Example:

```
mesh_guid
  → material_guid_1
  → material_guid_2

material_guid
  → texture_guid
```

Use cases:

* asset hot reload
* safe deletion
* build pipeline
* streaming

---

# 11. Import Pipeline

Importer steps:

```
FBX / GLB
  ↓
Assimp
  ↓
ImportScene
  ↓
Canonical writers
  ↓
.omesh
.mat
textures
  ↓
.meta generation
  ↓
AssetDatabase update
```

Important:

Importer generates `.meta` if missing.

---

# 12. Asset Loading Flow

Example runtime load.

Scene file:

```yaml
mesh: guid:9f4c2b3e8c1249e1a3d9c45c2c6a7e11
```

Runtime:

```
LoadMesh(guid)

AssetDatabase lookup
    ↓
Assets/Models/car.omesh
    ↓
MeshReader
    ↓
create GPU mesh
    ↓
load materials
    ↓
load textures
```

---

# 13. Asset Folder Layout

Professional project structure:

```text
Project/
 ├ Assets/
 │   ├ Models/
 │   ├ Materials/
 │   ├ Textures/
 │   ├ Animations/
 │
 ├ Library/
 │   └ asset_database.bin
 │
 ├ Cache/
 └ Logs/
```

Rule:

```
Assets  = user content
Library = engine generated
```

---

# 14. Hot Reload System

File watcher monitors:

```
Assets/
```

On change:

```
file modified
   ↓
reload asset
   ↓
update database
   ↓
notify editor
```

---

# 15. Runtime Reader Integration

Your existing readers become runtime loaders:

```
MeshReader
MaterialReader
TextureReader
AnimationReader
OeaReader
```

AssetManager simply calls them.

Example:

```cpp
Mesh LoadMesh(Guid id)
{
    auto path = database.GetPath(id);
    return MeshReader::Read(path);
}
```

---

# 16. Optional Future Systems (AAA features)

Do not implement yet but design for them.

### Asset bundles

```
group assets
stream at runtime
```

### Virtualized asset paths

```
guid → virtual path → physical path
```

### Derived data cache

```
compiled shaders
compressed meshes
texture mips
```

### Streaming

```
load LODs progressively
```

---

# 17. Final Module Layout

When finished the module should look like:

```text
assets/
 ├ guid/
 │   Guid.h
 │   Guid.cpp
 │
 ├ meta/
 │   MetaFile.h
 │   MetaFile.cpp
 │
 ├ database/
 │   AssetDatabase.h
 │   AssetDatabase.cpp
 │
 ├ runtime/
 │   AssetManager.h
 │   AssetManager.cpp
 │
 ├ cache/
 │   AssetCache.h
 │   AssetCache.cpp
 │
 ├ import/
 │   importer/
 │
 ├ readers/
 │
 ├ writers/
 │
 ├ formats/
```

---

# 18. Implementation Order (VERY IMPORTANT)

Do **not implement randomly**.

Correct order:

### Phase 1

```
GUID system
```

### Phase 2

```
.meta file system
```

### Phase 3

```
AssetDatabase
filesystem scan
registry cache
```

### Phase 4

```
runtime AssetManager
asset cache
```

### Phase 5

```
importer meta integration
```

### Phase 6

```
hot reload
dependency graph
```

---

# 19. Final Result

Your engine becomes:

```
Interchange formats
        ↓
Importer
        ↓
Canonical assets
        ↓
GUID asset system
        ↓
AssetDatabase
        ↓
Runtime asset manager
        ↓
Renderer / ECS / Physics
```

This is **exactly the architecture used in modern engines**.

---

# 20. Layered Architecture and Responsibility Split

**asset_pipe** owns identity, database, canonical formats, and **CPU asset data only**.  
**eyelib** owns GPU creation, audio, streaming, and shader compilation. No graphics API in asset_pipe.

## 3-layer pipeline

```text
Importer Layer
    ↓
Canonical Assets (.omesh .mat .png .anim)
    ↓
Asset Identity Layer (GUID, .meta, AssetDatabase, snapshot)
    ↓
Runtime Asset Layer (AssetManager, CPU cache, readers)
    ↓
Engine Runtime Systems (render, audio, streaming)
```

## Where each system lives

| System | Location | Responsibility |
|--------|----------|----------------|
| **asset_pipe** | core, meta, database, readers, runtime | Guid, AssetType, .meta, AssetDatabase, snapshot, dependency graph, CPU readers, AssetManager (CPU cache) |
| **Compression** | asset_pipe/core (or shared) | Compress/Decompress for importer, packages, streaming |
| **ShaderLoader** | eyelib/render | GUID → path → compile → GPU shader |
| **AudioLoader** | eyelib/audio | GUID → path → decode → AudioBuffer |
| **ShaderWatcher** | eyelib/tools (e.g. file_watcher) | Watch shader folder → reload |
| **StreamingManager** | eyelib (e.g. asset_runtime) | Async load, priority queue, memory budget; calls AssetManager underneath |
| **GPU resource creation** | eyelib/render | ImportMesh / LoadedTexture / LoadedMaterial → RenderDevice → GPU mesh/texture/material |
| **ModelBuilder** | eyelib/render | Assemble mesh + materials + textures; call AssetManager then RenderDevice |

## Module map

**asset_pipe**

- core: Guid, AssetType, (optional) Compression  
- meta: MetaFile  
- database: AssetDatabase, Snapshot  
- readers: MeshReader, MaterialReader, TextureReader, AnimationReader  
- runtime: AssetManager, CPU asset cache  

**eyelib**

- render: ShaderLoader, ModelBuilder, GPU resource creation  
- audio: AudioLoader  
- runtime: StreamingManager (above AssetManager)  
- tools: ShaderWatcher  

## Runtime flow

```text
Scene (entity + meshGuid)
    ↓
ModelBuilder / engine
    ↓
AssetManager::LoadMesh(guid)
    ↓
AssetDatabase → path → Readers → CPU asset (ImportMesh, etc.)
    ↓
RenderDevice::CreateMesh() / CreateTexture() / etc.
    ↓
GPU resource
```

**Rule:** asset_pipe returns CPU data only; eyelib converts CPU data → GPU/audio resources.

---

If you want, I can also show you the **single biggest mistake almost every custom engine makes in asset systems** (and how to avoid it). It will save you **months of refactoring later.**
