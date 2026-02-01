# OpenEye Player

Unified runtime loader that runs any scene + assets using the safe `api/` layer.

## Build

The player is built when `OPENEYE_BUILD_PLAYER` is ON (default).

## Run

Basic:

```
OpenEyePlayer --scene scenes/main.json --assets assets
```

With config:

```
OpenEyePlayer --config app/player/configs/dev.json
OpenEyePlayer --config app/player/configs/shipped.json
```

CLI overrides config:

```
OpenEyePlayer --config app/player/configs/dev.json --scene scenes/alt.json
```

## Common flags

- `--scene <path>`: scene JSON file.
- `--assets <path>`: asset root (repeatable).
- `--config <path>`: JSON config file.
- `--profile <name>`: `dev` or `shipped`.
- `--overlay`: lightweight on-screen overlay.
- `--stats`: log fps/entity stats every second.
- `--strict-assets`: fail on missing/unknown assets.
- `--headless`: no window; simulation only.
- `--fixed <seconds>`: fixed timestep.
- `--time-mode <realtime|fixed|replay>`.
- `--time-scale <n>`.

## Exit codes

- `0` OK
- `2` Invalid args
- `3` Config error
- `4` Engine init failed
- `5` Scene missing
- `6` Scene load failed
- `7` Strict asset failure
- `8` Window init failed
- `9` Render init failed
- `10` Physics init failed

## Files

- `main.cpp`: minimal entry.
- `PlayerApp.*`: core runtime flow.
- `PlayerConfig.*`: args + config parsing.
- `AssetBinding.*`: asset resolution/loading.
- `Overlay.*`: ImGui overlay.
- `configs/`: dev + shipped presets.
