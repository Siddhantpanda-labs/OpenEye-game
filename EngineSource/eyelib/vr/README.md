# VR Module

XR abstraction and optional runtimes for stereo/VR rendering.

## Interface

- **IXRProvider** – Session lifecycle, view count, per-view pose/FOV, frame submit. No OpenXR or other SDK types; engine code depends only on this.
- **XRTypes** – `XRViewData` (pose + FOV), `XRTextureHandle`.
- **NullXRProvider** – Stub implementation (no session). Use when no VR runtime is present.

## XR Camera entity

In the world/API, an **XRCamera** component marks an entity as the VR camera. When an XR session is active and the scene has an XR Camera entity (primary), the render path uses the XR provider for view count and per-view pose/projection instead of the normal Camera.

## Adding a runtime (e.g. OpenXR)

1. Implement `IXRProvider` in a separate target (e.g. `EyelibVROpenXR`).
2. Link that target to the OpenXR loader and use the OpenXR SDK only there.
3. At runtime, create the OpenXR provider instead of `NullXRProvider` when VR is requested; the rest of the engine stays unchanged.
