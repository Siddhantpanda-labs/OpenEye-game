Good approach 👍 — separating **phase 1 (pure conversion)** from **phase 2 (asset identity + metadata)** is exactly how many pipelines are built initially.
Right now the importer should behave like a **deterministic converter**:

```
Source scene file
(FBX / GLTF / GLB / DAE / BLEND)
        ↓
Scene parser
        ↓
Extract meshes / materials / textures / animations
        ↓
Convert to engine intermediate representation
        ↓
Write canonical binary files
(.mesh .mat .texture .anim)
```

No GUIDs yet. Just **clean canonical assets**.

---

# 1️⃣ First Rule of a Good Importer

Do **not** write a separate importer for each format.

Instead:

```
File format parser
        ↓
Unified scene representation
        ↓
Canonical asset writer
```

So the architecture is:

```
AssetImporter
      │
      ├── SceneLoader (Assimp / tinygltf)
      │
      ├── SceneConverter
      │
      ├── MeshWriter
      ├── MaterialWriter
      ├── TextureWriter
      └── AnimationWriter
```

This is how professional importers are structured.

---

# 2️⃣ Use a Scene Loader Library

For multi-format support use **Assimp** initially.

It supports:

```
fbx
gltf
glb
dae
blend
obj
```

Your importer will look like:

```cpp
Assimp::Importer importer;

const aiScene* scene =
    importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_OptimizeMeshes);
```

This converts the file into a **standard scene structure**.

---

# 3️⃣ Convert to Internal Representation

Do not directly write `.mesh` from Assimp structures.

Create your own intermediate types.

Example:

```cpp
struct ImportVertex
{
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    Vec4 tangent;
};

struct ImportMesh
{
    string name;

    vector<ImportVertex> vertices;
    vector<uint32_t> indices;

    int materialIndex;
};
```

And materials:

```cpp
struct ImportMaterial
{
    string name;

    string baseColorTexture;
    string normalTexture;

    float metallic;
    float roughness;
};
```

Now convert:

```cpp
ImportMesh mesh;

mesh.name = aiMesh->mName.C_Str();
mesh.vertices = ...
mesh.indices = ...
```

This isolates your engine from the source format.

---

# 4️⃣ Mesh Extraction

For each mesh in the scene:

```cpp
for (uint32_t i = 0; i < scene->mNumMeshes; i++)
{
    aiMesh* ai_mesh = scene->mMeshes[i];

    ImportMesh mesh;

    mesh.name = ai_mesh->mName.C_Str();

    // vertices
    for (uint32_t v = 0; v < ai_mesh->mNumVertices; v++)
    {
        ImportVertex vert;

        vert.position = ...
        vert.normal   = ...
        vert.uv       = ...

        mesh.vertices.push_back(vert);
    }

    // indices
    for (uint32_t f = 0; f < ai_mesh->mNumFaces; f++)
    {
        aiFace face = ai_mesh->mFaces[f];

        mesh.indices.push_back(face.mIndices[0]);
        mesh.indices.push_back(face.mIndices[1]);
        mesh.indices.push_back(face.mIndices[2]);
    }
}
```

---

# 5️⃣ Material Extraction

Example:

```cpp
for (uint32_t i = 0; i < scene->mNumMaterials; i++)
{
    aiMaterial* mat = scene->mMaterials[i];

    ImportMaterial material;

    aiString name;
    mat->Get(AI_MATKEY_NAME, name);

    material.name = name.C_Str();
}
```

Extract textures:

```
AI_MATKEY_TEXTURE_DIFFUSE
AI_MATKEY_TEXTURE_NORMALS
AI_MATKEY_TEXTURE_METALNESS
```

---

# 6️⃣ Naming Canonical Files

Use deterministic naming.

Example:

```
car.fbx
```

Meshes inside:

```
Body
Wheel
Glass
```

Output:

```
car_body.mesh
car_wheel.mesh
car_glass.mesh
```

Materials:

```
car_paint.material
car_rubber.material
```

Recommended rule:

```
<sourceName>_<nodeName>.mesh
```

Always lowercase and sanitized.

Example code:

```cpp
string outputName =
    sourceName + "_" + sanitize(mesh.name) + ".mesh";
```

---

# 7️⃣ Write Canonical Binary Files

Create writers:

```
MeshWriter
MaterialWriter
TextureWriter
AnimationWriter
```

Example `.mesh` writer:

```cpp
void WriteMesh(const ImportMesh& mesh, const string& path)
{
    File file(path, WriteBinary);

    MeshHeader header;

    header.vertexCount = mesh.vertices.size();
    header.indexCount  = mesh.indices.size();

    file.write(header);

    file.write(mesh.vertices);
    file.write(mesh.indices);
}
```

Example header:

```cpp
struct MeshHeader
{
    uint32 magic = 0x4D455348; // "MESH"
    uint32 version = 1;

    uint32 vertexCount;
    uint32 indexCount;
};
```

---

# 8️⃣ Output Directory Structure

Example:

```
Assets/
   models/
      car_body.mesh
      car_wheel.mesh

   materials/
      car_paint.material
```

Textures copied to:

```
Assets/textures/
```

---

# 9️⃣ Animation Extraction

If skeleton exists:

```
aiScene->mAnimations
```

Convert into:

```
.anim
```

Structure:

```
AnimationHeader
Channels
Keyframes
```

---

# 🔟 Logging

Every importer should produce logs.

Example:

```
[Importer] Loaded car.fbx
[Importer] Meshes found: 3
[Importer] Materials found: 2
[Importer] Writing car_body.mesh
```

This helps debugging.

---

# 11️⃣ Expected Result

Input:

```
car.fbx
```

Output:

```
car_body.mesh
car_wheel.mesh
car_glass.mesh

car_paint.material
car_rubber.material
```

Each file is canonical binary.

---

# 12️⃣ First Milestone

Your importer should achieve:

```
any supported format
        ↓
parse scene
        ↓
extract meshes + materials
        ↓
write canonical files
```

No metadata, no registry yet.

That comes later.

---

# 13️⃣ Difficulty

This importer stage is roughly:

```
1500–2500 lines of code
```

Most of it is **data conversion**, not complicated algorithms.

---

# Final Advice

Do **not over-engineer yet**.

Focus on:

```
1️⃣ reliable scene parsing
2️⃣ deterministic file naming
3️⃣ stable binary format writing
```

Once that works, then we add:

```
GUID system
Asset registry
Reimport pipeline
Dependency tracking
```

---

Great — this is one of the **most important parts of a professional importer**.
Most beginner importers fail here because they ignore the **scene graph**.

3D files (FBX, GLTF, DAE, etc.) are **not just meshes**. They contain a **node hierarchy** with transforms, instancing, and references.

Professional engines must **flatten this hierarchy correctly**.

---

# 1️⃣ What a Scene Graph Looks Like

Inside a typical file:

```
Root
 ├── Car
 │     ├── BodyMesh
 │     ├── Wheel_FL
 │     ├── Wheel_FR
 │     ├── Wheel_RL
 │     └── Wheel_RR
 │
 └── Environment
        ├── Tree_01
        ├── Tree_02
        └── Tree_03
```

Each node has:

```
local transform
mesh reference
children
```

The mesh may be reused in multiple nodes.

---

# 2️⃣ Important Concept: Mesh ≠ Node

This is critical.

In most formats:

```
Node → references mesh
```

Example:

```
Wheel_FL node → wheel.mesh
Wheel_FR node → wheel.mesh
Wheel_RL node → wheel.mesh
Wheel_RR node → wheel.mesh
```

There is **one mesh**, but **four nodes**.

This is called **instancing**.

Professional engines preserve this.

---

# 3️⃣ Data Separation

During import you separate two things:

### Mesh data

```
geometry
vertices
indices
submeshes
```

### Scene structure

```
node hierarchy
transforms
mesh references
```

So the importer produces:

```
.mesh files
+
.scene representation
```

---

# 4️⃣ Assimp Node Structure

Assimp gives you:

```
aiScene
 └── aiNode (root)
       ├── children
       ├── transformation
       └── mesh indices
```

Each node:

```
node transform
list of meshes
child nodes
```

---

# 5️⃣ Flattening the Scene Graph

To convert it correctly:

You **recursively traverse the node tree**.

Example algorithm:

```cpp
void ProcessNode(aiNode* node, Mat4 parentTransform)
{
    Mat4 localTransform = Convert(node->mTransformation);

    Mat4 worldTransform = parentTransform * localTransform;

    for each meshIndex in node->mMeshes
    {
        aiMesh* mesh = scene->mMeshes[meshIndex];

        CreateSceneEntity(mesh, worldTransform);
    }

    for each child in node->mChildren
    {
        ProcessNode(child, worldTransform);
    }
}
```

This accumulates transforms correctly.

---

# 6️⃣ Why Transform Accumulation Matters

Example:

```
Car node
  transform = translate(10,0,0)

Wheel node
  transform = translate(1,0,2)
```

Actual wheel position is:

```
Car transform
+
Wheel transform
```

So the importer computes:

```
worldTransform = parent * local
```

---

# 7️⃣ Instancing

If multiple nodes reference the same mesh:

```
wheel.mesh
```

You do **not duplicate the mesh file**.

Instead:

```
wheel.mesh
```

Then nodes reference it:

```
Wheel_FL → wheel.mesh
Wheel_FR → wheel.mesh
```

This saves memory and matches real engines.

---

# 8️⃣ What the Importer Should Output

Your importer should output two types of data.

### Canonical assets

```
wheel.mesh
car_body.mesh
tree.mesh
```

### Scene structure

Example `.scene`:

```json
Entities:
  - name: Wheel_FL
    mesh: wheel.mesh
    transform: [...]

  - name: Wheel_FR
    mesh: wheel.mesh
```

Notice:

```
same mesh
multiple entities
```

---

# 9️⃣ Materials

Materials work similarly.

```
mesh → material index
```

Importer writes:

```
paint.material
rubber.material
glass.material
```

Meshes reference materials.

---

# 🔟 Example Final Import Result

Input:

```
car.fbx
```

Output:

```
Meshes
  car_body.mesh
  wheel.mesh
  glass.mesh

Materials
  paint.material
  rubber.material
  glass.material

Scene
  car.scene
```

Scene file contains nodes:

```
Car
 ├ body entity
 ├ wheel_FL entity
 ├ wheel_FR entity
 ├ wheel_RL entity
 └ wheel_RR entity
```

All referencing canonical meshes.

---

# 11️⃣ Why This Design Is Industry Standard

This approach allows:

```
mesh reuse
instancing
proper transforms
correct hierarchy
```

Without it you get:

```
duplicate meshes
broken transforms
incorrect scenes
```

---

# 12️⃣ Advanced Feature (Later)

Professional engines also handle:

```
node animation
skeleton binding
LOD groups
collision meshes
```

But the hierarchy traversal above is the **core foundation**.

---

# 13️⃣ Implementation Strategy

Your importer should follow this order:

```
1. Parse scene file
2. Extract meshes
3. Write .mesh files
4. Extract materials
5. Write .mat files (canonical binary)
6. Traverse node graph
7. Create scene entities
8. Write .scene file
```

---

# 14️⃣ Final Mental Model

Think of a 3D file as:

```
Geometry database
+
Scene graph
```

Your importer converts it into:

```
Engine assets
+
Scene description
```

---

Good — this is the **most important design decision in your importer**.
If you design the `.mesh` binary format correctly now, you **won’t need to rewrite it later** when you add animation, LODs, GPU streaming, etc.

I'll show you a **realistic industry-style mesh format**, simplified but future-proof.

---

# 1️⃣ Goals of a Good `.mesh` Format

A professional mesh format should support:

```
multiple submeshes
multiple vertex streams
materials
LOD support
skeleton (for animation)
GPU-friendly layout
fast loading
```

And be **binary** so loading is very fast.

---

# 2️⃣ High-Level Layout

Typical `.mesh` file layout:

```
MeshHeader
LOD Table
Submesh Table
Vertex Streams
Index Buffer
Skeleton (optional)
Bounding Volumes
```

Visually:

```
+----------------+
| MeshHeader     |
+----------------+
| LOD Table      |
+----------------+
| Submesh Table  |
+----------------+
| Vertex Streams |
+----------------+
| Index Buffer   |
+----------------+
| Skeleton Data  |
+----------------+
| Bounds         |
+----------------+
```

---

# 3️⃣ Mesh Header

The header describes the file.

Example:

```cpp
struct MeshHeader
{
    uint32 magic;        // "MESH"
    uint32 version;

    uint32 lodCount;
    uint32 submeshCount;

    uint32 vertexCount;
    uint32 indexCount;

    uint32 vertexStreamCount;

    uint32 skeletonOffset;
};
```

Example values:

```
magic = 0x4D455348
version = 1
lodCount = 1
submeshCount = 3
vertexCount = 12456
indexCount = 37800
```

---

# 4️⃣ LOD Table

Supports multiple LODs.

Example:

```cpp
struct MeshLOD
{
    uint32 indexStart;
    uint32 indexCount;

    float screenSize;
};
```

Example:

```
LOD0 → full detail
LOD1 → medium
LOD2 → low
```

Your renderer can switch LODs based on distance.

---

# 5️⃣ Submesh Table

Each submesh corresponds to a material.

Example:

```cpp
struct SubMesh
{
    uint32 indexStart;
    uint32 indexCount;

    uint32 materialIndex;
};
```

Example:

```
Submesh 0 → car_paint.material
Submesh 1 → rubber.material
Submesh 2 → glass.material
```

This lets one mesh use multiple materials.

---

# 6️⃣ Vertex Streams

Modern engines separate vertex data into **streams**.

Example:

```
Stream 0 → positions
Stream 1 → normals + tangents
Stream 2 → UVs
Stream 3 → skin weights
```

Example descriptor:

```cpp
struct VertexStream
{
    uint32 offset;
    uint32 stride;
    uint32 elementCount;
};
```

This allows flexible layouts.

---

# 7️⃣ Vertex Structure (Typical)

Example vertex layout:

```cpp
struct Vertex
{
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
};
```

For skeletal meshes:

```cpp
struct SkinnedVertex
{
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;

    uint16 boneIndices[4];
    float  boneWeights[4];
};
```

---

# 8️⃣ Index Buffer

Indices are stored after vertex streams.

Example:

```
uint32 indices[indexCount];
```

Usually:

```
32-bit indices
```

or

```
16-bit if vertexCount < 65535
```

---

# 9️⃣ Bounding Volumes

These help with rendering culling.

Example:

```cpp
struct Bounds
{
    float3 center;
    float3 extents;

    float radius;
};
```

Used for:

```
frustum culling
LOD selection
physics
```

---

# 🔟 Skeleton Section (Optional)

If mesh is animated.

Example:

```cpp
struct Bone
{
    char name[64];

    int parentIndex;

    float4x4 bindPose;
};
```

And a list of bones:

```
Bone[boneCount]
```

---

# 11️⃣ GPU Alignment

To avoid slow GPU uploads, align buffers.

Example:

```
vertex buffer aligned to 16 bytes
index buffer aligned to 16 bytes
```

Many engines use:

```
16-byte alignment
```

for SIMD/GPU efficiency.

---

# 12️⃣ Example File Layout

Example `.mesh`:

```
MeshHeader
LODTable
SubmeshTable
VertexStreamTable

PositionStream
NormalStream
UVStream

IndexBuffer

SkeletonData
Bounds
```

---

# 13️⃣ Why This Format Is Good

This layout supports:

```
static meshes
skeletal meshes
LOD meshes
instancing
GPU streaming
```

Without redesigning the format later.

---

# 14️⃣ Runtime Loading

Runtime loader does:

```cpp
LoadMesh(file)
{
    read header
    read submeshes
    read vertex streams
    read indices

    create GPU buffers
}
```

Very fast.

No parsing.

---

# 15️⃣ Example Import Result

Input:

```
car.fbx
```

Importer outputs:

```
car_body.mesh
car_wheel.mesh
```

Inside `.mesh`:

```
header
submeshes
vertex buffers
index buffer
bounds
```

---

# 16️⃣ Estimated Size

Typical sizes:

```
10k vertices mesh ≈ 500 KB
```

Binary format keeps assets compact.

---

# 17️⃣ Future Extensions

Your format can later support:

```
meshlets
ray tracing BVH
nanite-like clusters
streaming chunks
```

without breaking compatibility.

---

# Final Advice

Your `.mesh` format should prioritize:

```
simple binary structure
GPU-friendly vertex layout
submesh support
future skeletal support
```

The design above is **very close to what modern engines use**.

---

Good — now let’s design the **full importer pipeline** the way professional engines do it. The goal is that **any supported source scene (FBX, GLTF, GLB, DAE, BLEND)** goes through **one unified pipeline** and produces all canonical assets in **one deterministic pass**.

I’ll show you the **architecture, stages, and data flow**.

---

# 1️⃣ High-Level Import Pipeline

Professional engines structure importers like this:

```
Source File
(FBX / GLTF / BLEND / DAE)
        │
        ▼
Scene Loader
(Assimp / GLTF loader)
        │
        ▼
Intermediate Scene Representation
        │
        ▼
Asset Extraction Stage
        ├── Mesh extraction
        ├── Material extraction
        ├── Texture extraction
        ├── Animation extraction
        │
        ▼
Canonical Asset Writers
        ├── .mesh
        ├── .material
        ├── .texture
        ├── .anim
        │
        ▼
prefab Builder
        │
        ▼
.prefab file
```

Everything happens **in one pipeline run**.

---

# 2️⃣ Stage 1 — Scene Loading

The first stage reads the source file and produces a **normalized scene graph**.

Use a loader like:

* Assimp
* tinygltf

Example:

```cpp
Assimp::Importer importer;

const aiScene* scene = importer.ReadFile(
    path,
    aiProcess_Triangulate |
    aiProcess_GenNormals |
    aiProcess_CalcTangentSpace |
    aiProcess_JoinIdenticalVertices |
    aiProcess_ImproveCacheLocality
);
```

Output:

```
aiScene
 ├── meshes
 ├── materials
 ├── textures
 ├── animations
 └── node hierarchy
```

---

# 3️⃣ Stage 2 — Intermediate Scene Representation

You **must convert** the library structures into your own types.

This isolates your engine from external formats.

Example:

```cpp
struct ImportScene
{
    vector<ImportMesh> meshes;
    vector<ImportMaterial> materials;
    vector<ImportTexture> textures;
    vector<ImportAnimation> animations;

    ImportNode root;
};
```

Example node:

```cpp
struct ImportNode
{
    string name;

    Mat4 transform;

    vector<int> meshIndices;

    vector<ImportNode> children;
};
```

Now your importer is **format-independent**.

---

# 4️⃣ Stage 3 — Mesh Extraction

Extract mesh geometry.

```
aiScene → ImportMesh
```

Example:

```cpp
struct ImportMesh
{
    string name;

    vector<Vertex> vertices;
    vector<uint32> indices;

    vector<SubMesh> submeshes;

    int materialIndex;
};
```

Steps performed here:

```
triangulation
normal generation
tangent generation
vertex deduplication
index creation
```

Optional:

```
vertex cache optimization
bounding volume generation
```

---

# 5️⃣ Stage 4 — Material Extraction

Materials are extracted separately.

Example structure:

```cpp
struct ImportMaterial
{
    string name;

    string baseColorTexture;
    string normalTexture;
    string metallicTexture;

    float roughness;
    float metallic;
};
```

The importer resolves texture references.

Example:

```
albedo.png
normal.png
```

These become canonical `.texture` assets later.

---

# 6️⃣ Stage 5 — Texture Extraction

Textures referenced by materials must be copied or converted.

Example:

```
albedo.png
normal.png
```

Importer converts them into canonical assets:

```
car_albedo.texture
car_normal.texture
```

Later you may add:

```
compression
mipmaps
```

---

# 7️⃣ Stage 6 — Animation Extraction

If animations exist:

```
aiScene->mAnimations
```

Convert to:

```cpp
struct ImportAnimation
{
    string name;

    float duration;

    vector<AnimationChannel> channels;
};
```

Each channel represents a bone transform track.

Output:

```
walk.anim
idle.anim
```

---

# 8️⃣ Stage 7 — prefab Graph Traversal

Now we process the node hierarchy.

Example prefab graph:

```
Root
 ├ Car
 │   ├ BodyMesh
 │   ├ Wheel_FL
 │   └ Wheel_FR
```

Traverse recursively:

```cpp
ProcessNode(node, parentTransform)
```

Compute world transforms:

```
worldTransform = parent * local
```

Create scene entities referencing meshes.

---

# 9️⃣ Stage 8 — Canonical Asset Writers

Now write all extracted data into engine formats.

Example:

```
MeshWriter
MaterialWriter
TextureWriter
AnimationWriter
```

Example output:

```
car_body.mesh
car_wheel.mesh

car_paint.material
rubber.material

car_albedo.texture
car_normal.texture

walk.anim
```

These are **binary files**.

---

# 🔟 Stage 9 — Prefab Builder

Finally generate a Prefab file.

Example `.prefab`:

```json
Entities:
  - name: CarBody
    mesh: car_body.mesh
    transform: [...]

  - name: Wheel_FL
    mesh: car_wheel.mesh
    transform: [...]

  - name: Wheel_FR
    mesh: car_wheel.mesh
    transform: [...]
```

Notice mesh reuse.

---

# 11️⃣ One Import Pass Example

Input:

```
car.fbx
```

Importer outputs:

```
Meshes
   car_body.mesh
   car_wheel.mesh

Materials
   paint.material
   rubber.material

Textures
   car_albedo.texture
   car_normal.texture

Scene
   car.scene
```

Everything generated in **one run**.

---

# 12️⃣ Why This Pipeline Works Well

Advantages:

```
format independent
deterministic outputs
easy reimport
asset reuse
clean separation of data
```

It also supports future systems like:

```
LOD generation
collision meshes
meshlets
streaming
```

---

# 13️⃣ Typical Importer Code Structure

Your importer module might look like:

```
AssetImporter/
 ├ SceneLoader.cpp
 ├ SceneConverter.cpp
 ├ MeshExtractor.cpp
 ├ MaterialExtractor.cpp
 ├ TextureExtractor.cpp
 ├ AnimationExtractor.cpp
 ├ MeshWriter.cpp
 ├ MaterialWriter.cpp
 ├ TextureWriter.cpp
 └ SceneWriter.cpp
```

Each stage is isolated.

---

# 14️⃣ Performance Strategy

Good importers process large scenes efficiently.

Typical optimizations:

```
parallel mesh extraction
parallel texture conversion
parallel asset writing
```

Not required initially.

---

# 15️⃣ Typical Importer Size

A good importer pipeline usually ends up around:

```
3000–6000 lines of code
```

depending on features.

---

# Final Insight

Think of the importer as a **compiler for 3D assets**.

```
Source 
      ↓
Importer pipeline
      ↓
Engine-ready binary assets
```

The runtime should never know about FBX, GLTF, etc.

Only canonical formats.

---

# 1️⃣ Two Different Concepts

### Engine Scene (Level)

This is what your editor edits.

Example:

```
garage.scene
city.scene
level1.scene
```

Contains:

```
entities
lights
cameras
scripts
physics objects
references to meshes/materials
```

This is the **world file**.

---

### Imported Scene Layout

When importing an FBX you might get a hierarchy like:

```
Car
 ├ Body
 ├ Wheel_FL
 ├ Wheel_FR
```

That is **not a level**, it is an **asset hierarchy**.

This is usually treated as a **prefab**.

---

# 2️⃣ What Professional Engines Do

Engines separate these two concepts.

| Concept                 | Unreal    | Unity     |
| ----------------------- | --------- | --------- |
| Level                   | `.umap`   | `.unity`  |
| Prefab / Imported asset | `.uasset` | `.prefab` |

Examples:

* Unreal Engine

  * level = `.umap`
  * imported assets = `.uasset`

* Unity

  * level = `.unity`
  * imported model hierarchy = `.prefab`

---

# 3️⃣ What Your Engine Should Do

Keep `.scene` for **editor worlds**.

Example:

```
Assets/scenes/city.scene
Assets/scenes/garage.scene
```

For imported layouts use something like:

```
.prefab
.model
.asset
.blueprint
```

Best option:

```
.prefab
```

Example:

```
car.prefab
tree.prefab
house.prefab
```

---

# 4️⃣ Import Result Example

Importing:

```
car.fbx
```

Should produce:

```
car_body.mesh
wheel.mesh
paint.material
rubber.material
car.prefab
```

`car.prefab` contains:

```
Car
 ├ Body entity
 ├ Wheel_FL entity
 ├ Wheel_FR entity
```

---

# 5️⃣ How It Is Used

Then in your **level scene**:

```
garage.scene
```

You can place:

```
car.prefab
car.prefab
car.prefab
```

Each instance references the same assets.

---

# 6️⃣ Example Structure

Project:

```
Assets/
   meshes/
   materials/
   textures/
   prefabs/
   scenes/
```

Example:

```
Assets/
   meshes/
       car_body.mesh
       wheel.mesh

   materials/
       paint.material

   prefabs/
       car.prefab

   scenes/
       garage.scene
```

---

# 7️⃣ Why This Separation Matters

Without this separation:

```
scene file
    ↓
becomes both asset and level
```

That causes problems:

* can't reuse imported models easily
* difficult asset organization
* harder asset referencing

Prefab solves this.

---

# 8️⃣ What `.prefab` Contains

A prefab file usually contains:

```
entity hierarchy
transforms
mesh references
material references
```

Example:

```json
Prefab
{
  entities:
  [
    {
      name: "Body",
      mesh: "car_body.mesh"
    },
    {
      name: "Wheel_FL",
      mesh: "wheel.mesh"
    }
  ]
}
```

---

# 9️⃣ Editor Behavior

Editor workflow becomes:

```
Asset Browser
    ↓
drag prefab
    ↓
create entity instance
```

Like Unity.

---

# 🔟 Final Recommendation

Introduce:

```
.prefab
```

for imported object hierarchies.

So your engine uses:

```
.scene  → world / level
.prefab → reusable asset hierarchy
.mesh   → geometry
.mat → material
.texture → texture
.anim → animation
```

This structure scales well.

---

---

