# Editor Panels

Individual ImGui panels for world editing.

## Panels

- `HierarchyPanel.h/cpp` - Scene hierarchy tree view
- `InspectorPanel.h/cpp` - Component inspector and editing
- `AssetPanel.h/cpp` - Asset browser and management
- `ScenePanel.h/cpp` - Scene management (save/load)
- `AnimationPanel.h/cpp` - Animation timeline editor
- `MaterialPanel.h/cpp` - Material editor
- `GameLogicPanel.h/cpp` - Game logic configuration

## Panel Interface

All panels should implement:
```cpp
class IEditorPanel {
public:
    virtual void Render() = 0;
    virtual const char* GetName() const = 0;
    virtual bool IsOpen() const { return m_isOpen; }
    virtual void SetOpen(bool open) { m_isOpen = open; }
protected:
    bool m_isOpen = true;
};
```

