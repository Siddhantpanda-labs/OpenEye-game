# OpenEye - Multiplayer VR City Sandbox Engine

A professional-grade 3D game engine built with pure OpenGL, designed for creating large-scale multiplayer VR city sandbox experiences.

## Features

- **Pure OpenGL 4.5 Rendering** - High-performance deferred rendering pipeline
- **Large Scene Support** - Efficient culling, LOD, instancing, and batching for city-scale worlds
- **Post-Processing Pipeline** - HDR, tone mapping, bloom, SSAO, SSR, and anti-aliasing
- **Render-to-Texture** - Video playback and future browser rendering on 3D quads
- **Bullet Physics** - Full physics integration for realistic interactions
- **Lua Scripting** - Hot-reloadable scripting system for game logic
- **Development Editor** - Visual world building tools with ImGui
- **VR Ready** - Designed for VR integration from the ground up

## Building

### Prerequisites

- CMake 3.20 or later
- Visual Studio 2019 or later (with C++20 support)
- Windows 10 or later

### Build Steps

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build . --config Debug
```

The executables will be in `build/bin/Debug/`:
- `Editor.exe` - Development environment with UI tools
- `Game.exe` - Final game runtime (no editor tools)

### CMake Options

- `OPENEYE_BUILD_EDITOR` (ON by default) - Build the development editor
- `OPENEYE_BUILD_GAME` (ON by default) - Build the final game executable
- `CMAKE_BUILD_TYPE` - Debug or Release

## Project Structure

```
OpenEye/
├── src/                    # Source code
│   ├── core/              # Core systems (window, input, time, logging)
│   ├── ecs/               # Entity Component System (EnTT)
│   ├── rendering/         # Rendering systems (to be implemented)
│   ├── physics/           # Physics integration (to be implemented)
│   ├── scripting/         # Lua scripting (to be implemented)
│   ├── editor/            # Development editor tools
│   └── game/              # Game-specific code
├── assets/                # Game assets (models, textures, scripts, scenes)
├── thirdparty/            # Third-party libraries
└── CMakeLists.txt         # Build configuration
```

## Current Status

### ✅ Phase 0: Bootstrap & Build System (COMPLETE)
- Window management with GLFW
- OpenGL 4.5 context initialization
- Logging system
- Input handling
- Time management
- Basic ECS foundation

### 🚧 Next: Phase 1 - Resource & Shader System
- Shader manager with hot-reload
- Texture loading (stb_image)
- Mesh loading (Assimp)
- Material system

## Development Plan

See [plan.md](plan.md) for the complete sequential development plan.

## License

[Add your license here]

## Contributing

This is a professional development project. Code should be:
- Well-organized and documented
- Follow C++20 best practices
- Performance-conscious
- Maintainable and scalable

