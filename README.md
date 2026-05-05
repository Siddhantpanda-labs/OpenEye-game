# OpenEye

OpenEye is a **C++20 3D engine library** (`EngineSource/eyelib/`) with a **stable public API layer** (`EngineSource/api/`) and a set of apps/tests (`EngineSource/app/`) built on top of that API.

The long-term direction is an **AI-operable simulation world** where *all world edits* flow through a single **World Mutation System** (commands, validation, transactions, snapshots), enabling deterministic replay, tooling, and safe automation.

## Key ideas (how this repo is shaped)

- **API-first**: apps should include headers from `api/`, not `eyelib/`. The API uses an ABI-friendly style (PIMPL where appropriate).
- **One writer**: the world is mutated only through commands (editor/AI/gameplay/network are all *command producers*).
- **One world, many regions**: “games” are region-scoped rules/systems inside one continuous simulation world (arena inside a city, etc.).

## Repo layout

```
OpenEye/
├── EngineSource/    # Engine source (api, eyelib, app, thirdparty, assets)
│   ├── api/         # Public stable API surface (client-facing)
│   ├── eyelib/      # Engine/library implementation (internal)
│   ├── app/         # Apps built using api/ (editor, player, tests, experiments)
│   ├── assets/      # Engine shaders, default materials, icons
│   ├── thirdparty/  # Vendored dependencies (glfw, glad, imgui, Jolt, lua, ...)
│   └── cmake/
├── Engine/          # Built runtime (bin/, lib/) — output of build
├── Projects/        # User projects (e.g. MyWorld with Assets/, Cache/)
├── documentation/  # Project vision + architecture/implementation docs
├── build/           # CMake build directory
└── CMakeLists.txt
```

## Building

See `BUILD.md` for the authoritative build steps and troubleshooting.

### Prerequisites

- CMake 3.20+
- Visual Studio 2022 (recommended) or another C++20-capable toolchain
- Windows 10+

### Quick start (Windows / VS 2022)

```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug
```

### What gets built

- **Output**: Executables and libraries go to `Engine/bin/` and `Engine/lib/` (engine runtime layout).
- **Libraries**: `Eyelib*` modules + `Eyelib*API` modules (e.g. `EyelibWorld` and `EyelibWorldAPI`).
- **Executable(s)**:
  - `OpenEyePlayer` (from `EngineSource/app/player/`) when `OPENEYE_BUILD_PLAYER=ON` (default).
  - `OpenEyeAIPlayground` (from `EngineSource/app/ai_playground/`) when `OPENEYE_BUILD_AI_PLAYGROUND=ON` (default): scene + chat shell for AI tooling; see `EngineSource/app/ai_playground/README.md`.
  - `OpenEyeRenderingSandbox` (from `EngineSource/app/rendering_sandbox/`) when `OPENEYE_BUILD_RENDERING_SANDBOX=ON` (default): isolated harness for `EyelibRendering` (`eyelib/rendering/`); does not link `EyelibRender`. Set `OPENEYE_BUILD_RENDERING_SANDBOX=OFF` to skip.
  - `OpenEyeApiTests` (from `EngineSource/app/test/`) when `OPENEYE_BUILD_TESTS=ON` and `app/test` exists.
  - A “game app” is scaffolded to live under `app/openmeta`, but game/editor targets are currently forced off in the root CMake configuration.

### Useful CMake options

- `OPENEYE_BUILD_EYELIB` (default ON): build the `eyelib/` + `api/` module set
- `OPENEYE_BUILD_AI_PLAYGROUND` (default ON): build `OpenEyeAIPlayground` (pulls in `app/player` for `OpenEyeGameRuntime`)
- `OPENEYE_BUILD_TESTS` (default ON): build `OpenEyeApiTests`
- `OPENEYE_USE_LZ4`, `OPENEYE_USE_ZSTD`: compression toggles used by serialization/assets

## Documentation (recommended reading order)

- `documentation/intro.md`: overall intent and architecture principles
- `documentation/World_kernel_Ai.md`: World Kernel requirements (determinism, commands, regions, introspection)
- `documentation/plans/world_mutation_system_implementation_plan.md`: concrete module layout + implementation progress
- `documentation/reports/Scripting_System_Assessment.md`: Lua scripting status and production gaps

## Current focus

The most important subsystem in this repo is the **World Mutation System** (`eyelib/world/` + `api/world/`):

- deterministic scheduling/time modes
- command buffer + validation
- transactions (undo/redo scaffolding)
- serialization/snapshots (single serializer pipeline)
- region support for “games inside the world”

## License

TBD (not yet specified).

## Contributing

- Keep `api/` stable; refactor freely behind it in `eyelib/`.
- Prefer deterministic, testable, data-driven systems (especially in world/serialization).

