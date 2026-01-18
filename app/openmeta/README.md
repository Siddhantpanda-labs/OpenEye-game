# OpenMeta Game Application

Professional AAA-level game structure for the Multiplayer VR City Sandbox.

## Directory Structure

```
openmeta/
├── src/                    # Game source code
│   ├── core/              # Game core systems (GameLoop, GameState, Application)
│   ├── gameplay/          # Gameplay systems (Combat, Weapons, Inventory)
│   ├── world/             # World management (City, Arenas, Streaming)
│   ├── player/            # Player systems (Controller, Movement, Stats)
│   ├── ai/                # AI systems (NPCs, Behavior Trees, Pathfinding)
│   ├── ui/                # UI/HUD systems (Menus, HUD, VR UI)
│   ├── networking/        # Multiplayer/Networking systems
│   ├── audio/             # Audio systems (Sound, Music, 3D Audio)
│   ├── scripting/         # Script integration (Lua bindings for game)
│   ├── data/              # Game data structures (Configs, Game Data)
│   └── utils/             # Game utilities (Helpers, Math, Tools)
├── resources/             # Game-specific resources
│   ├── scenes/           # Scene definitions (JSON)
│   └── config/           # Configuration files
├── scripts/               # Lua game scripts
├── CMakeLists.txt        # Build configuration
└── main.cpp              # Application entry point (moved to src/core/)
```

## Architecture Principles

1. **Separation of Concerns**: Each system is self-contained
2. **No Editor Dependencies**: Game code never depends on editor panels
3. **ECS-Based**: All game entities use ECS architecture
4. **Script-Driven**: Gameplay logic in Lua scripts
5. **Data-Driven**: Configuration and game data in JSON/resources

## Build Configuration

- **Release Build**: Game only, no editor tools
- **Debug Build**: Game + Editor tools available

