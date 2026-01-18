# OpenMeta Project Structure

## Overview

Professional AAA-level game structure with clean separation between game code and editor tools.

## Directory Structure

```
app/
├── openmeta/              # Game Application (Always Built)
│   ├── src/               # Game source code
│   │   ├── core/          # Game core systems
│   │   ├── gameplay/      # Gameplay systems
│   │   ├── world/         # World management
│   │   ├── player/        # Player systems
│   │   ├── ai/            # AI systems
│   │   ├── ui/            # UI/HUD systems
│   │   ├── networking/    # Multiplayer systems
│   │   ├── audio/         # Audio systems
│   │   ├── scripting/    # Script integration
│   │   ├── data/          # Game data structures
│   │   └── utils/         # Game utilities
│   ├── resources/         # Game resources
│   │   ├── scenes/        # Scene definitions
│   │   └── config/        # Configuration files
│   ├── scripts/           # Lua game scripts
│   ├── CMakeLists.txt     # Game build config
│   └── main.cpp           # Game entry point
│
└── editor/                # Editor System (Debug Only)
    ├── panels/            # ImGui panels
    ├── utils/             # Editor utilities
    ├── gizmos/            # Transform gizmos
    └── CMakeLists.txt     # Editor build config
```

## Build Configuration

### Release Build
- ✅ **Game (OpenMeta)**: Built and included
- ❌ **Editor**: Excluded

### Debug Build
- ✅ **Game (OpenMeta)**: Built and included
- ✅ **Editor (OpenMetaEditor)**: Built and available

## Architecture Principles

### 1. Clean Separation
- **Game code** (`app/openmeta/`) never depends on editor
- **Editor code** (`app/editor/`) controls game through ECS
- No circular dependencies

### 2. ECS-Based
- All game entities use ECS architecture
- Editor edits entities through ECS components
- Game systems operate on ECS entities

### 3. Script-Driven
- Gameplay logic in Lua scripts (`scripts/`)
- Script bindings in `src/scripting/`
- Hot-reload in development

### 4. Data-Driven
- Configuration files in `resources/config/`
- Scene definitions in `resources/scenes/`
- Game data structures in `src/data/`

### 5. Modular Organization
- Each system in its own folder
- Clear responsibilities per system
- Easy to extend and maintain

## System Responsibilities

### Core Systems (`src/core/`)
- Game application lifecycle
- Game loop (update/render)
- Game state management
- Configuration loading

### Gameplay Systems (`src/gameplay/`)
- Combat mechanics
- Weapon systems
- Inventory management
- Damage and health
- Scoring system

### World Management (`src/world/`)
- City generation
- Arena management
- World streaming
- Portal/teleportation
- Spawn points

### Player Systems (`src/player/`)
- Player controller
- Movement system
- VR controller support
- Player stats
- Camera management

### AI Systems (`src/ai/`)
- Behavior trees
- Pathfinding
- Perception system
- NPC management
- Squad coordination

### UI Systems (`src/ui/`)
- HUD rendering
- Menu system
- VR UI
- In-world UI elements

### Networking (`src/networking/`)
- Client-server architecture
- Entity replication
- Client prediction
- Server reconciliation

### Audio (`src/audio/`)
- Sound effects
- Music system
- 3D positional audio

### Scripting (`src/scripting/`)
- Lua integration
- Script bindings
- Game script API

### Editor (`app/editor/`)
- ImGui panels (hierarchy, inspector, asset browser)
- Transform gizmos
- Scene editing tools
- **Debug builds only**

## Development Workflow

1. **Development (Debug Build)**
   - Game runs with editor panels available
   - Edit world through ImGui panels
   - Test gameplay immediately
   - Hot-reload scripts

2. **Testing (Release Build)**
   - Game runs without editor
   - No development overhead
   - Production-like performance

3. **Deployment**
   - Only game executable and assets
   - No editor tools included
   - Optimized for end users

## Next Steps

1. Implement core game systems
2. Create gameplay systems
3. Build world management
4. Add player systems
5. Implement AI systems
6. Create UI systems
7. Build editor panels (debug only)

