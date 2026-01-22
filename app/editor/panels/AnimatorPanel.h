#pragma once

#include "IEditorPanel.h"
#include "ecs/api/Scene.h"
#include "ecs/Components.h"

namespace OpenMeta {
namespace Editor {

class AnimatorPanel : public IEditorPanel {
public:
    explicit AnimatorPanel(OpenEye::ECS::Scene* scene) : m_scene(scene) {}
    void Render() override;
    const char* GetName() const override { return "Animator"; }

private:
    OpenEye::ECS::Scene* m_scene = nullptr;
};

} // namespace Editor
} // namespace OpenMeta

