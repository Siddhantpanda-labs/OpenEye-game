# Editor + Player Migration Track (Short)

## Status

- `M0` Baseline + audit: **Done**
- `M1` `api/rendering` facade: **Done**
- `M2` Editor scene-view migration: **Done**
- `M3` Editor migration remainder: **Done**
- `M4` Player + `OpenEyeGameRuntime` migration: **In progress**
- `M5` Legacy decommission (`api/render` + `eyelib/render`): **Not started**

## Done so far

- New rendering facade (`api/rendering`) is active and buildable.
- Editor migrated to new pipeline path (scene/game/material preview paths landed).
- Major player/runtime migration pieces landed (new pipeline path, cache wiring, runtime bridge updates).
- Orientation and play/stop game-view parity fixes landed.

## Next TODOs

1. Finish remaining `M4.0*` items:
   - `M4.0b` Skybox creation/cache API path
   - `M4.0c` Real `RenderTarget` wrapper path
   - `M4.0d` Real fence API path
2. Complete `M4` runtime closeout validation (not only compile checks).
3. Run canonical mesh pipeline validation on new renderer:
   - Stop/debug camera view
   - Play game panel view
   - Standalone player parity
4. Start `M5` only after `M4` validation is fully closed.

## Notes

- Keep comments and tracking concise.
- Keep `eyelib/rendering` module boundaries respected during migration work.
