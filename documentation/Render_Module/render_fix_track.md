# Render fix track - Sponza texture problem (new rendering module only)

## Scope

- Active implementation scope:
  - `EngineSource/eyelib/rendering/**`
  - `EngineSource/app/rendering_sandbox/**`
- Out of scope:
  - `EngineSource/eyelib/render/**` (deprecated)
  - Editor application changes

## Problem

- In `OpenEyeRenderingSandbox`, Sponza geometry renders, but textures/material appearance is mostly flat/creamy.
- Target behavior: textured output parity with expected Sponza material look.

## Tracking status

- Status: Open
- Owner area: New rendering module + sandbox import path

## Investigation checklist

1. Validate sandbox material texture discovery across all relevant Assimp texture channels.
2. Validate embedded texture decode path (compressed + uncompressed) and source resolution reliability.
3. Validate bindless texture handle allocation success rate for loaded material textures.
4. Validate material ID propagation:
   Assimp mesh/submesh material index -> `MeshDescriptor::submeshes` -> raster draw override -> shader material row.
5. Validate `MaterialTable` CPU->GPU upload coverage and bounds for all referenced material IDs.
6. Validate shader sampling contract vs CPU material packing contract (handle slots and channel usage).

## Rules for fixes

- Keep feature/module boundaries intact:
  - sandbox prepares import inputs,
  - `RenderResources` owns resource/upload lifecycle,
  - passes consume blackboard-published resources.
- No ad-hoc bypass paths in pass execution.
- No regressions to upload-phase discipline or frame-graph ownership/barrier design.

## Notes

- Old editor + deprecated render behavior may be used only as a reference oracle during investigation.
- Any adopted behavior must be implemented in new-module architecture terms, not copied as legacy coupling.
