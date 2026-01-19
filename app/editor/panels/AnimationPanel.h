#pragma once

#include "IEditorPanel.h"
#include "ecs/api/Scene.h"
#include "animation/AnimationSystem.h"

namespace OpenMeta {
namespace Editor {

/**
 * @brief Animation panel - control animation playback
 */
class AnimationPanel : public IEditorPanel {
public:
    AnimationPanel(OpenEye::ECS::Scene* scene);
    
    void Render() override;
    const char* GetName() const override { return "Animation"; }
    
private:
    void RenderAnimationControls(OpenEye::ECS::Entity entity, OpenEye::ECS::AnimationComponent& anim);
    
    OpenEye::ECS::Scene* m_scene = nullptr;
    int m_selectedEntityIndex = -1;
};

} // namespace Editor
} // namespace OpenMeta
