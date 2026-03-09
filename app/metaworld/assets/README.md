# Metaverse Game Assets

This directory contains all the assets for the Metaverse Game.

## Directory Structure

```
assets/
├── models/          # 3D models (.obj, .fbx, .gltf)
├── textures/        # Textures (.png, .jpg, .tga)
├── sounds/          # Audio files (.wav, .mp3, .ogg)
├── shaders/         # Custom shaders (.vert, .frag)
└── scenes/          # Scene files (.json, .xml)
```

## Asset Guidelines

- **Models**: Use standard formats like OBJ or GLTF
- **Textures**: Use PNG for transparency, JPG for photos
- **Sounds**: Use WAV for short sounds, MP3 for music
- **Shaders**: Follow Vulkan shader conventions

## Adding Assets

1. Place files in appropriate subdirectories
2. Update asset loading code to reference new files
3. Test assets in the game

## Asset Optimization

- Compress textures when possible
- Use LOD models for distant objects
- Optimize audio file sizes
- Use efficient shader code
