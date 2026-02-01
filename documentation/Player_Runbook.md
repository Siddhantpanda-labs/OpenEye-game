# OpenEye Player Runbook

This document describes how to build and run the unified runtime player (`OpenEyePlayer`) and how to use configs/profiles in development and shipping.

## What it is

`OpenEyePlayer` is the unified runtime loader. It boots the engine, loads a scene, binds assets, and runs the world simulation through the scheduler.

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

## Profiles

- `dev`: overlay + stats on, strict assets off.
- `shipped`: overlay + stats off, strict assets on, fixed timestep.

Set via:

```
--profile dev
--profile shipped
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

## File layout

- `app/player/main.cpp`: minimal entry.
- `app/player/PlayerApp.*`: core runtime flow.
- `app/player/PlayerConfig.*`: args + config parsing.
- `app/player/AssetBinding.*`: asset resolution/loading.
- `app/player/Overlay.*`: ImGui overlay.
- `app/player/configs/`: dev + shipped presets.
