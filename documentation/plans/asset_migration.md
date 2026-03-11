# Asset module migration: eyelib/assets → eyelib/asset_pipe + api

## Current state

- **eyelib/asset_pipe** is under `EngineSource/eyelib/asset_pipe` and built as part of the engine (Guid, AssetDatabase, AssetManager, meta, readers, writers, importer).
- **eyelib/geometry** now owns **EditableMesh**, **MeshBinaryFormat**, and **OEAFormat** (moved from eyelib/assets). Uses `asset_pipe::Guid` for skeleton/asset refs.
- **eyelib/assets** (legacy) is still present. It no longer contains EditableMesh, MeshBinaryFormat, or OEAFormat. Remaining: AssetId, AssetRegistry, AssetManager (eyelib), AssetCache, loaders, StreamingManager, compression. ShaderWatcher and assets::AudioClip moved out.

## Goal

Delete `EngineSource/eyelib/assets` and have all asset identity, database, and CPU loading go through **asset_pipe**; GPU/audio creation stays in **render** and **audio** (or api wrappers).

## Completed

1. **EditableMesh, MeshBinaryFormat, OEAFormat** → moved to `eyelib/geometry/`. Old files removed from assets.
2. **ShaderWatcher** → moved to `eyelib/render/`. Api re-exports `eyelib/render/ShaderWatcher.h`.
3. **Audio** → `eyelib/audio::AudioClip::LoadFromFile(path)` loads via platform filesystem + in-memory WAV parse. EyelibAudio no longer links EyelibAssets.
4. **api AssetRegistry on asset_pipe** → Registry uses `AssetDatabase` + `AssetManager`; descriptor caches with lazy load; `SetAssetRoot`, `Scan`, `Get*Descriptor`, `SetGPUResolver`; `GetMaterial` delegates to resolver.
5. **api IGPUResourceResolver** → `api/assets/IGPUResourceResolver.h`; player `RendererGPUResourceCache` implements it with `api::AssetRegistry*`; WorldRender/EditorSceneBuilder use registry for resolution.
6. **AssetDatabase** → `AddRuntimeAsset(path, type)` and `AddRuntimeAsset(path, type, guid)` for runtime registration.
7. **api::AssetId** → `operator==` / `operator!=` added for map keys.

## Migration steps (remaining, to remove eyelib/assets)

1. **Loaders** → Api loaders still delegate to eyelib loaders; switch to `registry->LoadAsset` or implement in api using asset_pipe.
2. **StreamingManager** → Move to api; use api registry / asset_pipe.
3. **Compression, eyelib IGPUResourceResolver** → Move or reimplement as needed.
4. **Remove eyelib/assets** → Remove subdirectory, delete folder, fix refs.

## Build order

- `eyelib/asset_pipe` is added before `api/assets` so api can link `AssetPipeCore`, `AssetPipeRuntime`, `AssetPipeDatabase` when migration is done.
- After migration, dependents of `EyelibAssets` (EyelibAudio, EyelibAnimation, EyelibGeometry, EyelibAssetsAPI, etc.) will depend on `AssetPipeRuntime` / `AssetPipeDatabase` / geometry / api as above.

## Reference

- Asset pipeline design: `documentation/plans/asset_pipeline.md`
- Current asset behaviour: `documentation/plans/current_asset.md`
