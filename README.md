# OpenEye

OpenEye is a **C++20 3D engine library** (`eyelib/`) with a **stable public API layer** (`api/`) and a set of apps/tests (`app/`) built on top of that API.

The long-term direction is an **AI-operable simulation world** where *all world edits* flow through a single **World Mutation System** (commands, validation, transactions, snapshots), enabling deterministic replay, tooling, and safe automation.

## Key ideas (how this repo is shaped)

- **API-first**: apps should include headers from `api/`, not `eyelib/`. The API uses an ABI-friendly style (PIMPL where appropriate).
- **One writer**: the world is mutated only through commands (editor/AI/gameplay/network are all *command producers*).
- **One world, many regions**: “games” are region-scoped rules/systems inside one continuous simulation world (arena inside a city, etc.).

## Repo layout

```
OpenEye/
├── api/             # Public stable API surface (client-facing)
├── eyelib/          # Engine/library implementation (internal)
├── app/             # Apps built using api/ (tests, experiments)
├── assets/          # Shaders, textures, scripts, sample assets
├── documentation/   # Project vision + architecture/implementation docs
├── thirdparty/      # Vendored dependencies (glfw, glad, imgui, bullet3, lua, ...)
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

- **Libraries**: `Eyelib*` modules + `Eyelib*API` modules (e.g. `EyelibWorld` and `EyelibWorldAPI`).
- **Executable(s)**:
  - `OpenEyeApiTests` (from `app/test/`) when `OPENEYE_BUILD_TESTS=ON` (default).
  - A “game app” is scaffolded to live under `app/openmeta`, but game/editor targets are currently forced off in the root CMake configuration.

### Useful CMake options

- `OPENEYE_BUILD_EYELIB` (default ON): build the `eyelib/` + `api/` module set
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

