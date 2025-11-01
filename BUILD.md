# Building OpenEye

## Quick Start

```powershell
# Create build directory
mkdir build
cd build

# Configure project (generates Visual Studio solution)
cmake .. -G "Visual Studio 17 2022" -A x64

# Or for Ninja (faster builds)
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build . --config Debug
```

## Output

After building, executables will be in:
- `build/bin/Debug/Editor.exe` - Development environment
- `build/bin/Debug/Game.exe` - Final game runtime

## Dependencies

All dependencies are included in `thirdparty/`:
- ✅ GLFW (window management)
- ✅ GLAD (OpenGL loading)
- ✅ GLM (math library)
- ✅ ImGui (UI framework)
- ✅ ImGuizmo (3D gizmos)
- ✅ Assimp (model loading)
- ✅ EnTT (ECS library)
- ✅ nlohmann/json (JSON parsing)
- ✅ stb (texture loading)

## Requirements

- **CMake**: 3.20 or later
- **Compiler**: Visual Studio 2019+ (MSVC) or Clang/GCC with C++20 support
- **Windows**: 10 or later
- **OpenGL**: 4.5 capable graphics driver

## Build Options

### CMake Cache Variables

- `CMAKE_BUILD_TYPE`: Debug or Release
- `OPENEYE_BUILD_EDITOR`: ON/OFF - Build editor executable
- `OPENEYE_BUILD_GAME`: ON/OFF - Build game executable

### Example: Build only Game

```powershell
cmake .. -DOPENEYE_BUILD_EDITOR=OFF -DOPENEYE_BUILD_GAME=ON
cmake --build . --config Release
```

## Troubleshooting

### Assimp DLL Not Found
The Assimp DLL (`assimp-vc143-mt.dll`) should be automatically copied to the output directory. If you see DLL errors:
- Verify the DLL exists at `thirdparty/assimp/bin/assimp-vc143-mt.dll`
- Check that the POST_BUILD command in CMakeLists.txt ran successfully

### OpenGL Context Creation Failed
- Update your graphics drivers
- Verify your GPU supports OpenGL 4.5 (check with GPU-Z or similar)
- Try disabling debug context: Edit `src/editor/main.cpp` and set `config.debugContext = false`

### Compilation Errors
- Ensure you're using C++20 compatible compiler
- Check that all third-party libraries are present in `thirdparty/`
- For MSVC, ensure `/permissive-` flag is working (try disabling it temporarily)

## Development Workflow

1. **Edit Code** - Modify source files in `src/`
2. **Rebuild** - Run `cmake --build . --config Debug`
3. **Run** - Execute `bin/Debug/Editor.exe` or `bin/Debug/Game.exe`
4. **Debug** - Attach debugger from Visual Studio or use your preferred IDE

## Next Steps

Once Phase 0 is complete and working:
- Phase 1: Implement shader and resource systems
- See `plan.md` for the full development roadmap

