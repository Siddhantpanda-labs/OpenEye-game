# Current Asset System — Engine and Pipeline

This document summarizes how the OpenEye engine handles assets today: the **eyelib/assets** runtime module, how the editor and player use it, and what the **asset_pipe** (importer and canonical formats) implements. It serves as a single reference for the current state before adding metadata (e.g. `.meta` + GUID) and deeper registry integration.

---

## 1. How the engine handles assets (overview)

- **Runtime identity:** Assets are identified by **AssetId** (128-bit GUID-style). The **AssetRegistry** maps `AssetId ↔ file path` and stores metadata (type, dependencies, file size, etc.). The **AssetCache** holds loaded CPU/GPU data keyed by `AssetId`.
- **Loading:** Loading is type-driven. **AssetRegistry::LoadAsset(path, type)** (or **LoadAssetDescriptorOnly**) registers the path, generates or reuses an `AssetId`, then delegates to the appropriate **loader** (MeshLoader, TextureLoader, MaterialLoader, ShaderLoader, AnimationLoader, AudioLoader). Loaders parse the file, create descriptors and/or GPU resources, and store them in **AssetManager**’s registry and cache.
- **Resolution:** The **AssetManager** singleton holds the registry and a render device. Once **SetRenderDevice** is called, **GetMesh(id)**, **GetMaterial(id)**, etc. resolve an `AssetId` to a GPU instance (creating from stored descriptors on first use). Editor and game runtime both go through this path; the editor and player set the device and then call **BindAssets** (or equivalent) to load assets referenced by the world.
- **Formats:** The engine’s **eyelib** loaders currently support:
  - **Mesh:** Assimp (FBX/GLTF/etc.) at runtime for some paths; for “canonical” paths, **MeshLoader** uses **eyelib**’s own **OEA** (single-asset container) and **MeshBin** (`.mesh.bin` chunk format). These are **separate** from the **asset_pipe** canonical formats (see below).
  - **Material:** JSON `.mat` files (shader path, albedo, textures, etc.).
  - **Texture:** Image files (path-based).
  - **Shader:** `.vert` / `.frag` pairs or by name.
  - **Animation / Audio:** Via AnimationLoader / AudioLoader (path-based).
- **No .meta yet:** There are no per-file `.meta` files or persistent GUIDs on disk. AssetIds are generated at load time when **RegisterAsset(filePath, assetType)** is used; the registry can be saved/loaded as a single JSON file (**SaveToFile** / **LoadFromFile**) for persistence across sessions.

---

## 2. The `eyelib/assets/` module (contents and roles)

Location: **EngineSource/eyelib/assets/** (and **loaders/**, **compression/**).

### 2.1 Identity and metadata

- **AssetId.h / AssetId.cpp**  
  - 128-bit asset identifier (two 64-bit parts).  
  - **Generate()**, **FromString()**, **ToString()** (legacy GUID-style string).  
  - **Invalid()**, **IsValid()**, equality, **Hash** for use in maps.

- **AssetTypes.h**  
  - **AssetType** enum: Unknown, Mesh, Texture, Material, Shader, Audio, Animation, Prefab, Scene.  
  - **ToString()**, **AssetTypeFromString()** for serialization.

- **AssetRegistry.h / AssetRegistry.cpp**  
  - **AssetMetadata:** id, filePath, assetType, version, dependencies, dependents, name, author, description, fileSize, lastModified.  
  - **RegisterAsset(id, path, type)** or **RegisterAsset(path, type)** (generates id).  
  - **LoadAsset(path, type)** / **LoadAsset(id)**: register + load (full load via type-specific loader).  
  - **LoadAssetDescriptorOnly(path, type)** / **(id)**: register + load descriptor only (no GPU creation until first **Get***).  
  - **UnloadAsset**, **ReloadAsset**, **UnregisterAsset**, **UpdateVersion**.  
  - **GetMetadata**, **GetId**, **GetFilePath**, **GetAssetType**, **GetAssetSize**, **GetAssetMemoryUsage**.  
  - **AddDependency** / **RemoveDependency**, **GetDependencies** / **GetDependents**.  
  - **SaveToFile** / **LoadFromFile**: whole registry as JSON (path ↔ id, type, deps, etc.).  
  - **Cache()** exposes **AssetCache**; **GetAllIds**, **Clear**.

### 2.2 Cache and manager

- **AssetCache.h / AssetCache.cpp**  
  - Typed **Store\<T\>(id, shared_ptr, type)** and **Get\<T\>(id)**.  
  - **Has**, **Remove**, **Clear**.  
  - Thread-safe (mutex); used by loaders to store loaded assets.

- **AssetManager.h / AssetManager.cpp**  
  - Singleton **Get()**; holds **AssetRegistry** and render device.  
  - **SetRenderDevice(device)** / **IsReadyForResolution()**: required before **Get*** can create GPU instances.  
  - **Store*Descriptor** and **Register*Instance** (Shader, Mesh, Texture, Material): loaders fill these.  
  - **GetShader(id)**, **GetMesh(id)**, **GetTexture(id)**, **GetMaterial(id)**: resolve to GPU instance (from descriptor on first use if needed).  
  - **SetGPUResolver** / **GetGPUResolver**: optional renderer-owned resolver.  
  - **ClearDescriptorsAndInstances()**, **OnDeviceLost()**: clear GPU caches; descriptors can be kept for re-creation after device loss.

### 2.3 Loaders (EngineSource/eyelib/assets/loaders/)

- **MeshLoader**  
  - **ImportToCanonical(interchangePath, outputPath):** Assimp → single mesh → **OEA::WriteMeshToFile** (eyelib’s .oea, not asset_pipe’s).  
  - **LoadFromCanonical(device, path):** reads **OEA** (eyelib) or **MeshBin** (`.mesh.bin`); returns **render::Mesh**.  
  - **LoadFromFile(device, path):** Assimp direct (FBX/GLTF, etc.) → multiple meshes.  
  - **LoadToDecoded(path)**, **LoadAsync(..., callback)**, **CreateFromDecoded(device, data)**.  
  - **LoadMesh(path)** / **LoadMeshDescriptorOnly(path):** register with AssetManager and load; used by **AssetRegistry** when type is Mesh.

- **TextureLoader**  
  - **LoadTexture(path, isSRGB)**, **LoadTextureAsync(..., callback)**.  
  - **LoadTextureDescriptorOnly(path, isSRGB)**.  
  - Path-based; no canonical .tex in engine path (see asset_pipe for .tex).

- **MaterialLoader**  
  - **LoadMaterial(path)**, **LoadMaterialDescriptorOnly(path)**: JSON `.mat` (shader, albedo, textures, etc.).  
  - **SaveMaterial(path, material)**.  
  - Resolves texture paths relative to .mat directory; loads shaders/textures via registry.

- **ShaderLoader**  
  - Load by path (`.vert`/`.frag` pairs) or by name; descriptor-only and full load; stored in AssetManager.

- **AnimationLoader**  
  - **LoadAnimation(path)**; registers and loads animation data (skeleton/clip).

- **AudioLoader**  
  - **LoadAudio(path)**; registers and loads **AudioClip**.

- **ModelLoader**  
  - **LoadModel(path):** returns **LoadedModel** (meshes, materials, transforms, skeleton, animation).  
  - Uses **AssetManager** device; supports interchange and canonical paths.

### 2.4 Canonical formats inside eyelib (not asset_pipe)

- **OEAFormat.h / OEAFormat.cpp**  
  - **OEA** namespace: single-asset container format (magic "OEA\0", version, **AssetType**: Mesh, Skeleton, Animation, Material, Texture).  
  - **WriteToFile** / **ReadFromFile** (type + payload); **WriteMeshToFile** / **ReadMeshFromFile** (mesh payload).  
  - Used by **MeshLoader::ImportToCanonical** and **LoadFromCanonical**.  
  - This is **distinct** from **asset_pipe**’s multi-entry `.oea` package format.

- **MeshBinaryFormat.h / MeshBinaryFormat.cpp**  
  - **MeshBin** namespace: chunk-based `.mesh.bin` (VERT, INDX, SUBM, SKIN, etc.).  
  - **WriteToFile** / **ReadFromFile** (EditableMesh); **Serialize** / **Deserialize** (memory).  
  - **MeshLoader::LoadFromCanonical** uses this when the file is not eyelib OEA.

- **EditableMesh**  
  - CPU mesh representation; used by MeshBin and OEA mesh payload; can **CopyToRenderMesh** to create **render::Mesh**.

### 2.5 Other components

- **StreamingManager**  
  - **RequestAsset(path, priority)**, **CancelRequest**, **GetLoadProgress**, **IsLoaded**.  
  - **SetMemoryBudget** / **GetMemoryBudget**.  
  - Uses **AssetManager** registry and cache for actual load and memory usage.

- **ShaderWatcher**  
  - Hot-reload of shaders (file watch).

- **compression/AssetCompression**  
  - Compression utilities; used where needed (e.g. for cached/streamed data).

- **IGPUResourceResolver**  
  - Interface for renderer to provide GPU instances (used by AssetManager when **SetGPUResolver** is set).

---

## 3. How the current setup uses assets (editor, player, viewer)

### 3.1 Editor

- Holds an **eyelib::api::AssetRegistry** (wrapper around **AssetManager::Registry()**).
- **SetRenderDevice** on the registry so that **Get*** can resolve to GPU resources.
- **EditorSceneBuilder** builds the render scene from the world: it collects asset references from the world, **ResolveAssetPath** against asset roots, and calls **registry.LoadAsset(resolved, type)** (or descriptor-only where appropriate). Materials are resolved via **ResolveMaterialForScene** using **AssetManager::GetMaterial(coreId)**.

So the editor uses **path-based** asset references from the world, resolved against roots, and loads them through the **eyelib** AssetRegistry/AssetManager. It does **not** use asset_pipe readers or .oea/.omesh packages.

### 3.2 Player (game runtime)

- **GameRuntime** has **BindAssets(world, assetRoots, assetRegistry, allowGpuAssets)**.
- **AssetBinding.cpp** walks **world.CollectAssetReferences()**, resolves each ref against **assetRoots**, and calls **registry.LoadAssetDescriptorOnly(resolved, type)** for Mesh, Material, Animation, etc. (Script refs and builtin mesh IDs are skipped.)
- So the player also uses **path-based** refs and the **eyelib** registry; no asset_pipe.

### 3.3 Canonical viewer (asset_pipe only)

- **CanonicalViewer** does **not** use **eyelib** AssetRegistry or AssetManager for its main asset loading.
- It accepts **only** `.oea` or `.omesh` paths (single argument).
- **.oea:** Uses **asset_pipe::OeaReader::ReadIndex** and **ExtractAll** to a **temp directory** (OpenEyeOeaCache). Then uses **asset_pipe::PrefabReader::Read** on the extracted prefab, **asset_pipe::MeshReader::Read** (path), **asset_pipe::MaterialReader::Read** (path), **asset_pipe::TextureReader::Read** (path when material path ends in .tex), and builds **eyelib::api::Mesh** via **BuildApiMeshFromImportMesh** (asset_pipe **ImportMesh** → API mesh). So for .oea, the **runtime** format is asset_pipe (prefab, mesh, material, texture readers); GPU side is still **eyelib** (api::Mesh, api::Material, api::Texture).
- **.omesh:** No extraction. **asset_pipe::OeaReader::ReadIndex** and **ReadEntryData** read prefab and mesh entries from the package; **PrefabReader::ReadFromText** and **MeshReader::Read(bytes)** work in memory. Materials come from **materialmap** (in package or next to .omesh) and are read from disk or from package bytes (**MaterialReader::Read(path)** or **Read(bytes)**). Textures are loaded from the **folder** (e.g. `textures/*.png`) via **api::Texture::LoadFromFile** or **TextureReader::Read** if path is .tex.
- So the canonical viewer is the **only** app that uses **asset_pipe** formats and readers; it turns them into eyelib API types for rendering only.

---

## 4. What is implemented in asset_pipe

**asset_pipe** is the **offline import and canonical format** pipeline. It produces files that the **CanonicalViewer** can open; the rest of the engine (editor/player) currently uses **eyelib** loaders and path-based refs.

### 4.1 Layout

- **asset_pipe/core/**  
  - Format headers and import types: **mesh_format.h**, **material_format.h**, **texture_format.h**, **oea_format.h**; **import_scene.h**, **import_mesh.h**, **import_material.h**, **import_texture.h**, **import_animation.h**, **import_node.h**, **math_types.h**.

- **asset_pipe/readers/**  
  - **MeshReader**: **Read(path)** and **Read(bytes)** → **ImportMesh**.  
  - **MaterialReader**: **Read(path)** and **Read(bytes)** → **LoadedMaterial**.  
  - **TextureReader**: **Read(path)** → **LoadedTexture** (decodes to pixels; supports .tex encoded payload).  
  - **PrefabReader**: **Read(path)** and **ReadFromText(text)** → **PrefabDrawable** list (mesh path + world transform).  
  - **OeaReader**: **ReadIndex(packagePath)** → **OeaEntry** list; **ExtractAll(packagePath, outDir)**; **ReadEntryData(packagePath, entryName)** → bytes (with zstd decompression when applicable).

- **asset_pipe/writers/**  
  - **MeshWriter**: **Write(ImportMesh, path)** → canonical `.mesh` (MeshFormat layout).  
  - **MaterialWriter**: **Write(ImportMaterial, path)** → canonical `.material` (MaterialFormat).  
  - **TextureWriter**: **Write(ImportTexture, path)** → canonical `.tex` (not used by current importer; see below).  
  - **PrefabWriter**: **Write(ImportScene, path)** → `.prefab` (YAML-like entity list).  
  - **AnimationWriter**: **Write(ImportAnimation, path)** → `.anim`.  
  - **OeaWriter**: **Write(packagePath, entries)**; each entry can be **sourcePath** or in-memory **payload**; compresses with zstd when available.

- **asset_pipe/tools/importer/**  
  - **SceneLoader**: Assimp load (FBX, GLTF, GLB, DAE, etc.).  
  - **SceneConverter**: Assimp scene → **ImportScene** (meshes, materials, nodes, animations); **ExtractTextures** (embedded + file; remap material paths to `textures/<name>.<ext>`).  
  - **TextureExtraction**: stb_image; encoded or decoded; **outputFileName** (e.g. `.png`); material refs become `textures/...`.  
  - **main.cpp**: CLI importer.

### 4.2 Importer behavior (modes)

- **Default:**  
  - Output: **&lt;out&gt;/&lt;modelName&gt;/** with **&lt;name&gt;.omesh** (package containing meshes, prefab, materialmap, and optionally embedded materials), **textures/** (loose PNG/JPEG, etc.), **&lt;name&gt;.anim** (loose).

- **--mat:**  
  - Same folder structure plus **materials/** with loose **.mat** files; .omesh contains materialmap pointing at **materials/*.mat**.

- **--oea:**  
  - Single file **&lt;out&gt;/&lt;modelName&gt;.oea** only (no loose files, no temp extraction). Package contains meshes, prefab, materialmap, materials, textures (encoded image bytes under `textures/...`), animations.

### 4.3 Canonical formats (asset_pipe)

- **.mesh:** Binary (MeshFormat): header (vertex/index/submesh counts, offsets), LOD table, submesh table, vertex/index data, bounds. Used inside .oea/.omesh and for loose mesh writes.
- **.material / .mat:** Binary (MaterialFormat): header (path lengths, metallic/roughness, albedoColor R/G/B), then name and texture paths. Materials reference **textures/&lt;name&gt;.png** (or similar), not .tex, in the current importer output.
- **.tex:** Binary (TextureFormat): optional encoded image storage; reader decodes to pixels. **TextureWriter** exists but the importer does **not** write .tex files; it writes loose images or embeds encoded bytes in .oea.
- **.prefab:** YAML-like text (prefab writer): entity list with name, transform, mesh path. Stored inside .oea/.omesh; viewer uses **PrefabReader::Read** or **ReadFromText**.
- **.anim:** Binary (animation writer): ANI1 magic, channels (bone name, position/rotation/scale keys). Written loose or into .oea.
- **.materialmap:** Text: `index|path` per line; maps submesh material index to material file (e.g. `materials/foo.mat` or embedded).
- **.oea (asset_pipe):** Multi-entry package (OeaFormat): header (magic, version, entry count), TOC (per entry: type, nameLen, flags, offset, size, uncompressedSize), then payloads (optionally zstd-compressed). Entry types: Mesh, Material, Texture, Animation, Prefab, MaterialMap. Used for **--oea** output and for **.omesh** (same layout; .omesh is the same package format used as “model package” with prefab + meshes + materialmap + optional materials).
- **.omesh:** Same container format as .oea; typically contains one prefab, N meshes, one materialmap, and optionally material entries. No extraction to temp; viewer reads entries in memory.

### 4.4 What the importer does not use

- **TextureWriter** is not used; textures are written as loose images or as raw encoded bytes in the package.
- **.tex** is not produced by the importer; the viewer can still load a .tex if a material path pointed to one (current material paths point to textures/*.png etc.).
- **MeshWriter** is not used for loose files; meshes are serialized in memory and written into .oea/.omesh via **OeaWriter**.

---

## 5. Summary table

| Concern | eyelib/assets | asset_pipe |
|--------|----------------|------------|
| **Purpose** | Runtime load, registry, cache, GPU resolution | Offline import, canonical formats, package layout |
| **Identity** | AssetId (128-bit), path ↔ id in registry | Path-based inside packages; no GUID on disk yet |
| **Mesh** | OEA (eyelib single-asset), MeshBin (.mesh.bin), Assimp | .mesh (MeshFormat), inside .oea/.omesh |
| **Material** | JSON .mat | Binary .material/.mat (MaterialFormat) |
| **Texture** | Path → image load | Loose images or encoded in .oea; .tex optional |
| **Prefab** | — | .prefab (text), inside .oea/.omesh |
| **Animation** | AnimationLoader (path) | .anim (binary), loose or in .oea |
| **Package** | OEA (single type + payload) | .oea/.omesh (multi-entry, zstd optional) |
| **Used by** | Editor, Player, loaders | CanonicalViewer, importer CLI |

---

## 6. How assets are managed and called in Editor and Player (implementation)

This section describes how the **editor** and **player** apps create, own, and use the asset registry and loaders in code. It clarifies where the registry is used versus where path-based loading and local caches are used.

### 6.1 Registry ownership and API wrapper

- **Editor:** `EditorApp` creates `eyelib::api::AssetRegistry assetRegistry` on the stack, calls `assetRegistry.SetRenderDevice(editorRenderDevice)`, and stores a pointer in `EditorContext::assetRegistry`. The **api::AssetRegistry** class is a thin wrapper: it holds a reference to `eyelib::assets::AssetManager::Get().Registry()` (the single global core registry). So all editor asset registration and lookup go through that one registry.
- **Player:** `PlayerApp` also creates `eyelib::api::AssetRegistry assetRegistry` on the stack and passes it to `GameRuntime::Initialize(world, assetRegistry, options.assetRoots, ...)`. The same **AssetManager** singleton and its registry are used; the player never creates a separate registry.
- **Render device:** Both apps call `assetRegistry.SetRenderDevice(device)` so that later `AssetManager::GetMaterial(id)` / `GetMesh(id)` can resolve descriptors to GPU instances. The device is set before scene build so that material (and optional mesh) resolution works.

### 6.2 Asset roots

- **Editor:** `ctx.assetRoots` is initialized to `{ engineAssets }` (e.g. `Engine/assets`). When a project is opened (`OpenProject`), it is set to `{ ctx.projectAssetsRoot }` (e.g. project folder’s `Assets` from the `.openeye` descriptor). `CloseProject` restores `assetRoots = { engineAssetsPath }`. Path resolution is always “try path as-is, then try each root + path” via `EditorSceneBuilder::ResolveAssetPath(id, assetRoots)` (and the same pattern in the player).
- **Player:** `options.assetRoots` comes from config (e.g. `dev.json` / `shipped.json`: `"assetRoots": ["assets"]`) or CLI; if empty, it defaults to the engine assets directory. Roots are normalized and passed into `GameRuntime::Initialize` and then into `BindAssets`, `BuildScene`, scripting, and skybox resolution.

### 6.3 Editor: when and how assets are loaded

- **BindSceneAssets** (called when loading a scene, opening a project, or via menu “Refresh”): Iterates `world.CollectAssetReferences()`, maps each ref to a type (Mesh, Material, Animation), resolves path with `ResolveAssetPath(ref.id, assetRoots)`, and calls **`registry.LoadAsset(resolved, type)`**. So the editor **fully loads** every referenced asset (mesh, material, animation) into the registry up front. Loaders run immediately and may create GPU resources (depending on loader and device).
- **Build** (every frame for scene view / game view): For each renderable, the **mesh** path is resolved with `ResolveAssetPath(meshId, assetRoots)` and then **`LoadModelEntry(meshResolved)`** is called. `LoadModelEntry` does **not** use the registry: it calls **`eyelib::api::ModelLoader::LoadModel(path)`** and caches the result (mesh + material + skeleton) in **EditorSceneBuilder::m_modelCache**. So in the editor, **meshes are loaded by path via ModelLoader and cached in the scene builder**; they are not fetched by AssetId from the registry at build time. For **materials**, if `renderable.materialAssetId` is set, the code resolves the path and then calls **`ResolveMaterialForScene(assetRegistry, matResolved)`**, which does **`registry.GetId(pathOrId)`** (or `AssetId::FromString` if it looks like a GUID string) and then **`AssetManager::Get().GetMaterial(coreId)`**. So **materials are resolved through the registry** (path → AssetId → cached material). Skybox is path-resolved and loaded with **TextureCube::LoadFromFiles**, then cached in **m_skyboxCache** (no registry).
- **Takeaway:** In the editor, the registry is used to (1) pre-load all refs via **LoadAsset** when **BindSceneAssets** runs, and (2) resolve **materials** by path → AssetId → GetMaterial. **Meshes** are not loaded by AssetId; they are loaded by path via **ModelLoader** and cached in **EditorSceneBuilder**.

### 6.4 Player: when and how assets are loaded

- **BindAssets** (inside `GameRuntime::Initialize`): Iterates `world.CollectAssetReferences()`, resolves path with `ResolveAssetPath(ref.id, assetRoots)`, and calls **`registry.LoadAssetDescriptorOnly(resolved, type)`** for Mesh, Material, Animation, etc. So the player **only loads descriptors** (and registers path → AssetId); GPU resources are created later on first **Get***. After that, **`assetRegistry.SetRenderDevice(m_render->GetDevice())`** is called so that **GetMaterial** / **GetMesh** can create instances from descriptors.
- **BuildScene** (WorldRenderBridge): For each mesh renderer, the **mesh** path is resolved and **`LoadModelEntry(meshResolved)`** is used. As in the editor, **`LoadModelEntry`** calls **`ModelLoader::LoadModel(path)`** and caches the result in **WorldRenderBridge::m_modelCache**. So the player **also does not use the registry for mesh loading at render time**; it uses path → ModelLoader and its own cache. For **materials**, the code uses **`assetRegistry->GetId(matResolved)`** and then **`ResolveMaterial(matId)`**, which is **`AssetManager::Get().GetMaterial(coreId)`**. So **materials** are resolved via registry (path → AssetId → GetMaterial). The pipeline uses **RendererGPUResourceCache**, which implements **IGPUResourceResolver**: when the renderer needs a mesh or material by AssetId, the cache uses **AssetManager**’s descriptors (or legacy **Cache().Get**) and creates GPU resources on demand. In practice, **BuildScene** gets meshes from **LoadModelEntry** (ModelLoader + cache), not from **registry.GetId(meshPath)** + **GetMesh(id)**. So the registry is used for **material** (and **animation**, in **WorldAnimationBridge**) resolution, but **meshes** in the render path are still loaded by path through **ModelLoader** and **WorldRenderBridge::m_modelCache**.
- **Takeaway:** In the player, the registry holds path → id and descriptors (from **LoadAssetDescriptorOnly**). **Materials** (and animations) are resolved via registry. **Meshes** at render time are loaded by **path** via **ModelLoader** and cached in **WorldRenderBridge**; they are not looked up by AssetId in the registry during **BuildScene**.

### 6.5 Single global registry

- **api::AssetRegistry** does not own a separate registry; it wraps **AssetManager::Get().Registry()**. So editor and player (and any other app using **api::AssetRegistry**) all share the same underlying **eyelib::assets::AssetRegistry** and **AssetCache** inside the **AssetManager** singleton. If the editor and player were to run in the same process (e.g. editor Play mode), they would share the same registry and cache.

### 6.6 Implications

- **Mesh loading is path-based and duplicated:** Both editor and player resolve mesh paths and then call **ModelLoader::LoadModel(path)** with a local cache (**m_modelCache**). The registry’s **LoadAsset** / **LoadAssetDescriptorOnly** for type Mesh does run (so mesh paths get an AssetId and a descriptor in the registry when the loader supports it), but **Build** / **BuildScene** do not use **GetMesh(registry.GetId(path))**; they use **LoadModelEntry(path)**. So mesh data can effectively live in two places: the registry/cache (from BindSceneAssets/BindAssets) and the scene builder’s model cache. Unifying on “resolve path → GetId → GetMesh(id)” would make mesh loading go through the registry consistently.
- **Material loading is registry-centric:** Materials are consistently resolved by path → **GetId** → **GetMaterial(id)** in both editor and player, so the registry is the single source for material instances once loaded.
- **Asset roots drive all path resolution:** Every asset path from the world (mesh, material, animation, skybox, script) is resolved against **assetRoots** before use. There is no GUID-based reference in the world yet; all refs are string paths (or path-like ids).

---

## 7. Next steps (context for metadata work)

- **Metadata system:** Introduce per-file (and optionally per-folder) `.meta` with **GUID** and type; generate/update from importer and optionally from editor. Keep **AssetId** and registry; optionally persist GUID in .meta and derive AssetId from it or keep current id generation and add GUID to metadata.
- **Unify loading (optional):** Allow editor/player to load **asset_pipe** packages (.oea/.omesh) via a thin adapter that uses **asset_pipe** readers and then registers/creates resources in **AssetRegistry**/AssetManager, so one canonical path works in both viewer and editor/runtime.
- **TextureWriter:** Either wire importer to emit .tex when desired or leave as optional for other tools; current pipeline is consistent without it.

This document reflects the state of the codebase at the time of writing; for exact APIs and file paths, refer to the sources listed above.
