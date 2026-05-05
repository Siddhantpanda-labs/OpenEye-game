# `EyelibRendering` (`eyelib/rendering/`)

Next-gen render module (sandboxed). **No** `#include` of the legacy include root (see `CMakeLists.txt` firewall).

**Directory layout** matches `documentation/Render_Module/new_render_module.md` (top-level roles):

| Area | Path |
|------|------|
| RHI | `rhi/` (`Vulkan/`, empty `OpenGL/`, `diligent/` placeholders; CMake builds Vulkan only) |
| Scene contract | `scene/` (`IRenderSceneView`, `RenderTypes`, `CameraView`, `CameraConventions`, `DrawList`, …) |
| Core frame infra | `core/` (`RenderWorld`, `FrameContext`, `RenderContext`, `RenderCapabilities`, `RenderResources`, `PassPushConstants`, `RenderBackend`, …) |
| Frame graph | `framegraph/` |
| Scheduler + features | `scheduler/` |
| Visibility | `visibility/` |
| Geometry | `geometry/` (`MeshDescriptor*`, providers, …) |
| Materials | `materials/` |
| Shaders (CPU) | `shaders/` |
| Lighting | `lighting/` (`ibl/`, `shadows/`, `gi/`, `reflections/`, …) |
| Render features | `features/` (`raster/`, `atmosphere/`, `raytracing/`, …) |
| Temporal | `temporal/` (`upscaling/`, `denoising/`) |
| Post | `post/` |
| GPU resources | `resources/` (`Texture*`, `TextureDescriptor`, `Mesh`, `Shader`, …) |
| 2D | `2d/` |
| Debug | `debug/` |
| Backend helpers | `backend/` (policy until folded into doc layout) |

Many headers under `framegraph/` … `debug/` are **scaffolds** (minimal types); behavior is filled in as the doc roadmap is executed.

**Note:** `rhi/CreateDevice.h` re-exports `CreateVulkanDevice` only. `CMakeLists.txt` excludes `rhi/OpenGL/**` and `rhi/diligent/**` sources.
