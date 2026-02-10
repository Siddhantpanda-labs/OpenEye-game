# BGFX in thirdparty (manual setup)

You need the **bgfx.cmake** project and its three dependencies (bx, bimg, bgfx) under `thirdparty/bgfx.cmake/`. The final layout must be:

```
thirdparty/
  bgfx.cmake/          ← CMake build scripts (and cmake/ subdir)
    bx/                ← bx library source (include/, src/)
    bimg/              ← bimg library source (include/, src/)
    bgfx/              ← bgfx library source (include/, src/)
```

---

## Option A: Manual download (no git submodules)

If `git submodule update` does not work, download each repo as a **ZIP** and extract into the right folder.

### 1. Create the folder

From your repo root:

```
mkdir thirdparty\bgfx.cmake
cd thirdparty\bgfx.cmake
```

### 2. Download and extract each repo

Download the **ZIP** from each GitHub “Code” → “Download ZIP” link, then extract so that the **contents** of the zip (the folder with `include/`, `src/`, etc.) end up in the path shown.

| What to get | Download ZIP from | Extract so you have |
|-------------|-------------------|----------------------|
| **bgfx.cmake** (build scripts) | https://github.com/pr0g/bgfx.cmake/archive/refs/heads/main.zip | `thirdparty/bgfx.cmake/CMakeLists.txt`, `thirdparty/bgfx.cmake/cmake/`, etc. |
| **bx** | https://github.com/bkaradzic/bx/archive/refs/heads/master.zip | `thirdparty/bgfx.cmake/bx/include/`, `thirdparty/bgfx.cmake/bx/src/` |
| **bimg** | https://github.com/bkaradzic/bimg/archive/refs/heads/master.zip | `thirdparty/bgfx.cmake/bimg/include/`, `thirdparty/bgfx.cmake/bimg/src/` |
| **bgfx** | https://github.com/bkaradzic/bgfx/archive/refs/heads/master.zip | `thirdparty/bgfx.cmake/bgfx/include/`, `thirdparty/bgfx.cmake/bgfx/src/` |

**Step by step:**

1. **bgfx.cmake**  
   - Download: https://github.com/pr0g/bgfx.cmake/archive/refs/heads/main.zip  
   - Unzip. You get a folder like `bgfx.cmake-main`.  
   - Copy **everything inside** `bgfx.cmake-main` (CMakeLists.txt, cmake/, .gitmodules, etc.) into `thirdparty/bgfx.cmake/`.

2. **bx**  
   - Download: https://github.com/bkaradzic/bx/archive/refs/heads/master.zip  
   - Unzip. You get a folder like `bx-master`.  
   - Rename/copy that folder so the **contents** of `bx-master` (the `include/` and `src/` dirs) live in `thirdparty/bgfx.cmake/bx/`.  
   - So: `thirdparty/bgfx.cmake/bx/include/`, `thirdparty/bgfx.cmake/bx/src/` must exist.

3. **bimg**  
   - Download: https://github.com/bkaradzic/bimg/archive/refs/heads/master.zip  
   - Unzip → e.g. `bimg-master`.  
   - Put its contents into `thirdparty/bgfx.cmake/bimg/` so you have `thirdparty/bgfx.cmake/bimg/include/`, `thirdparty/bgfx.cmake/bimg/src/`.

4. **bgfx**  
   - Download: https://github.com/bkaradzic/bgfx/archive/refs/heads/master.zip  
   - Unzip → e.g. `bgfx-master`.  
   - Put its contents into `thirdparty/bgfx.cmake/bgfx/` so you have `thirdparty/bgfx.cmake/bgfx/include/`, `thirdparty/bgfx.cmake/bgfx/src/`.

### 3. Check the layout

You should have:

- `thirdparty/bgfx.cmake/CMakeLists.txt`
- `thirdparty/bgfx.cmake/cmake/` (folder with bx, bimg, bgfx CMake files)
- `thirdparty/bgfx.cmake/bx/include/` and `thirdparty/bgfx.cmake/bx/src/`
- `thirdparty/bgfx.cmake/bimg/include/` and `thirdparty/bgfx.cmake/bimg/src/`
- `thirdparty/bgfx.cmake/bgfx/include/` and `thirdparty/bgfx.cmake/bgfx/src/`

Then build with:

```bash
cmake -B build -DOPENEYE_USE_BGFX=ON
cmake --build build --config Debug
```

---

## Option B: Git + submodules

If you prefer git and submodules work:

```bash
cd thirdparty
git clone https://github.com/pr0g/bgfx.cmake.git
cd bgfx.cmake
git submodule update --init --recursive
```

Same layout as above; then run the same cmake/build commands.

---

## Build

Configure with BGFX enabled:

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64 -DOPENEYE_USE_BGFX=ON
cmake --build build --config Debug
```

The BGFX test app is **OpenEyeBGFXTest** (e.g. `build/bin/Debug/OpenEyeBGFXTest.exe`). It prefers Vulkan and, when Vulkan is used, loads the compiled copy shaders from `assets/shaders/bgfx/vulkan/` and draws a fullscreen quad. Run from `build/bin/Debug` after building and after running `python scripts/compile_bgfx_shaders.py` so the Vulkan .bin shaders are present.

### Note: bx patch for MSVC

On Windows, `thirdparty/bgfx.cmake/bx/src/thread.cpp` is patched so the `__try` block is in a separate helper with no C++ objects, avoiding MSVC C2712 (“Cannot use __try in functions that require object unwinding”). If you replace bx with a fresh copy, re-apply this patch or the build will fail on `thread.cpp`.
