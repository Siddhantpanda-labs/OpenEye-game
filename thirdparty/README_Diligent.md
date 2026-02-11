# Diligent Engine (DiligentCore) in thirdparty

When **OPENEYE_USE_DILIGENT** is ON, the build looks for **DiligentCore** in either `thirdparty/DiligentCore/` or `thirdparty/diligent/DiligentCore/`. DiligentCore is the graphics API abstraction (D3D11, D3D12, OpenGL, Vulkan, etc.) and is used by the RHI Diligent backend. See `documentation/plans/RHI_Diligent_Integration_Plan.md`.

**Note:** The option defaults to **OFF**, so you must pass `-DOPENEYE_USE_DILIGENT=ON` when configuring to build Diligent.

Required layout (one of):

```
thirdparty/
  DiligentCore/          ← DiligentCore repo (with submodules inited)
    CMakeLists.txt
    BuildTools/
    Common/
    Graphics/
    Platforms/
    Primitives/
    ThirdParty/
    ...
```

---

## Option A: Git clone + submodules

From the repo root:

```bash
cd thirdparty
git clone https://github.com/DiligentGraphics/DiligentCore.git
cd DiligentCore
git submodule update --init --recursive
```

---

## Option B: Add as git submodule to OpenEye

From the repo root:

```bash
git submodule add https://github.com/DiligentGraphics/DiligentCore.git thirdparty/DiligentCore
cd thirdparty/DiligentCore
git submodule update --init --recursive
```

---

## Build

Configure with Diligent enabled:

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64 -DOPENEYE_USE_DILIGENT=ON
cmake --build build --config Debug
```

If neither path contains DiligentCore, CMake will set **OPENEYE_DILIGENT_AVAILABLE=0** and the Diligent RHI backend will remain stub-only (no Diligent libraries linked). Selecting `RenderBackend::Diligent` at runtime still works but uses the stub device.

When DiligentCore is present and **OPENEYE_DILIGENT_AVAILABLE=1**, EyelibRender links against Diligent static libraries (OpenGL and D3D11 on Windows). The RHI `rhi/diligent/` implementation can then be extended to use the real Diligent API (Phase 2 of the integration plan).
