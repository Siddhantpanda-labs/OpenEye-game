# Editor System

Development editor - ImGui panels for world editing, separate from game code.

## Directory Structure

```
editor/
├── panels/           # Individual ImGui panels
│   ├── HierarchyPanel.h/cpp
│   ├── InspectorPanel.h/cpp
│   ├── AssetPanel.h/cpp
│   ├── ScenePanel.h/cpp
│   └── AnimationPanel.h/cpp
├── utils/           # Editor utilities
│   ├── EditorUtils.h/cpp
│   └── SelectionManager.h/cpp
├── gizmos/          # Transform gizmos
│   ├── TransformGizmo.h/cpp
│   └── GizmoRenderer.h/cpp
└── CMakeLists.txt  # Editor build configuration
```

## Architecture Principles

1. **Separate from Game**: Editor never included in release builds
2. **ECS-Based Editing**: Editor controls game through ECS
3. **Floating Panels**: ImGui panels are separate floating windows
4. **Debug Only**: Editor only builds in debug configuration
5. **No Game Dependencies**: Game code never depends on editor

## Build Configuration

- **Debug Build**: Editor available
- **Release Build**: Editor excluded

