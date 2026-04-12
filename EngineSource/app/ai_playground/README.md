# OpenEye AI Playground

**api/** client: 3D scene + chat panel. **Projects** live under **`Projects/AI_Playground/<name>/`** at the OpenEye workspace root (folder that contains `EngineSource` and `Projects`). Use **New Playground** / **Open Playground** in the top-left panel to create or open a project; each project gets `Assets/`, **`Scene/main.scene`**, `AI/`, and **`<name>.openeye`** (manifest; same base name as the project folder). **New Playground:** by default writes a minimal scene (`version`, `renderPipelineMode`, **`skybox":"evening"`** horizontal-cross cubemap); optionally check **Load packaged template** to copy **`Engine/assets/AI_Playground/default.scene`** (replace that file to customize the template). **Renderer:** defaults to **Vulkan** unless config/`--renderer` sets another backend. **Phase 1:** `/observe` (JSON world), `/plan` (batch JSON), `/ai demo`; tool catalog `documentation/Ai_plan/tools_catalog_v1.md`. **Phase 3+:** LLM tools.

**CLI:** Omit `--scene` to start with no scene and pick a playground in the UI. Pass `--scene <path>` to load a scene directly (same as player).

## Requirements

- Same as the unified player: Windows, Visual Studio 2022 or Ninja, CMake 3.20+, GPU/driver for OpenGL or Vulkan (see root `README.md`).
- Built when `OPENEYE_BUILD_AI_PLAYGROUND=ON` (default ON at repo root).

## Build

From the repo root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Output: `Engine/bin/OpenEyeAIPlayground.exe` (or `OpenEyeAIPlayground` with Ninja).

`OPENEYE_BUILD_AI_PLAYGROUND` also enables the `app/player` subdirectory so **OpenEyeGameRuntime** is available (the player `.exe` is still gated by `OPENEYE_BUILD_PLAYER`).

## Run

From a working directory where the **scene path** resolves (often the **repository root**):

```powershell
.\Engine\bin\OpenEyeAIPlayground.exe --scene scenes\main.json
```

Or pass an absolute path to any world scene file the engine can load (e.g. `.scene` or `.json`, same JSON format).

**Engine assets:** the app sets the process working directory to the engine root (folder containing `assets/shaders`) when possible, same as the player.

## World commands (chat)

Slash commands map to **`api::world::World`** only. Plain English gets a hint until the LLM loop is wired.

| Command | Action |
|--------|--------|
| `/help` | List commands |
| `/query` | Compact entity list (component **names** only) |
| `/observe` | JSON snapshot with per-entity **component data** (for model context) |
| `/plan <json-array>` | Batch steps: `create`, `destroy`, `set` (see `tools_catalog_v1.md`) |
| `/ai demo` | Idempotent demo: plane + cube + directional light (`AI_PG_*`) |
| `/create <name>` | `World::CreateEntity` |
| `/destroy <name>` | `World::DestroyEntity` (resolve by metadata name) |
| `/get <name> <Component>` | `World::GetComponentJson` |
| `/set <name> <Component> <json>` | `World::SetComponentJson` (remainder of line is JSON) |
| `/ask <goal>` | OpenAI Chat Completions + tools (Windows: WinHTTP). Requires **`OPENAI_API_KEY`**; optional **`OPENAI_MODEL`** (default `gpt-4o-mini`). |

**Tool catalog (prompts / grounding):** `documentation/Ai_plan/tools_catalog_v1.md`

## Options

Uses the same flags as **OpenEyePlayer** (see `PlayerConfig.cpp` / `--help`): `--scene`, `--assets`, `--renderer opengl|vulkan|...`, `--width`, `--height`, `--config`, etc.

Default window title: **OpenEye AI Playground**. Default log file: **OpenEyeAIPlayground.log**.

## Secrets (`/ask`)

Do **not** commit API keys. Set **`OPENAI_API_KEY`** in the environment before running the exe. Optional **`OPENAI_MODEL`** (defaults to `gpt-4o-mini`). HTTPS client: **WinHTTP** on Windows only; other platforms return a clear error until a portable client is added.

## Roadmap

`documentation/Ai_plan/AI_PLAYGROUND_ROADMAP.md`
