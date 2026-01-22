#include "AnimatorPanel.h"

#include "imgui.h"
#include <cstdio>
#include <glm/gtc/type_ptr.hpp>

#include "animation/controller/AnimatorController.h"

namespace OpenMeta {
namespace Editor {

using namespace OpenEye::ECS;
using namespace OpenEye::Animation::Controller;

void AnimatorPanel::Render() {
    if (!m_scene) return;
    ImGui::Begin("Animator", &m_isOpen);

    auto& registry = m_scene->GetRegistry();
    auto view = registry.view<AnimatorComponent>();

    if (view.empty()) {
        ImGui::Text("No AnimatorComponent entities in scene.");
        ImGui::End();
        return;
    }

    for (auto entity : view) {
        if (!registry.valid(entity)) continue;
        auto& animator = registry.get<AnimatorComponent>(entity);

        std::string name = "Entity";
        if (registry.all_of<Name>(entity)) name = registry.get<Name>(entity).name;

        ImGui::PushID((int)entity);
        if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            char buf[512]{};
            if (!animator.controllerPath.empty()) {
                std::snprintf(buf, sizeof(buf), "%s", animator.controllerPath.c_str());
            }
            if (ImGui::InputText("Controller Path", buf, sizeof(buf))) {
                animator.controllerPath = std::string(buf);
                // force reload on next update
                animator.controller.reset();
                animator.layers.clear();
            }
            ImGui::Checkbox("Apply Root Motion", &animator.applyRootMotion);
            ImGui::Checkbox("RootMotion -> Physics (kinematic)", &animator.rootMotionAffectsPhysics);

            // Controller info / layers
            auto ctrl = std::static_pointer_cast<AnimatorController>(animator.controller);
            if (ctrl) {
                ImGui::Separator();
                ImGui::Text("Controller: %s", ctrl->name.c_str());
                ImGui::Text("Layers: %zu", ctrl->layers.size());

                ImGui::Separator();
                ImGui::Text("Parameters");
                for (const auto& p : ctrl->params) {
                    ImGui::PushID(p.name.c_str());
                    switch (p.type) {
                        case ParamType::Bool: {
                            bool v = animator.params.GetBool(p.name);
                            if (ImGui::Checkbox(p.name.c_str(), &v)) {
                                animator.params.SetBool(p.name, v);
                            }
                        } break;
                        case ParamType::Int: {
                            int v = animator.params.GetInt(p.name);
                            if (ImGui::DragInt(p.name.c_str(), &v, 1.0f)) {
                                animator.params.SetInt(p.name, v);
                            }
                        } break;
                        case ParamType::Float: {
                            float v = animator.params.GetFloat(p.name);
                            if (ImGui::DragFloat(p.name.c_str(), &v, 0.01f, -10.0f, 10.0f, "%.3f")) {
                                animator.params.SetFloat(p.name, v);
                            }
                        } break;
                        case ParamType::Trigger: {
                            if (ImGui::Button(p.name.c_str())) {
                                animator.params.SetTrigger(p.name);
                            }
                            ImGui::SameLine();
                            ImGui::TextDisabled("(trigger)");
                        } break;
                    }
                    ImGui::PopID();
                }

                for (size_t li = 0; li < animator.layers.size(); ++li) {
                    const auto& lr = animator.layers[li];
                    ImGui::Text("Layer %zu: state='%s' time=%.2f%s", li, lr.currentState.c_str(), lr.stateTime,
                                lr.inTransition ? " (blending)" : "");
                }
            } else {
                ImGui::Text("Controller not loaded yet (will load at runtime when scene updates).");
            }

            // Events
            if (!animator.recentEvents.empty()) {
                ImGui::Separator();
                ImGui::Text("Recent Events");
                for (int i = (int)animator.recentEvents.size() - 1; i >= 0; --i) {
                    const auto& ev = animator.recentEvents[size_t(i)];
                    ImGui::Text("[%s] %s (%s) t=%.2f", ev.clip.c_str(), ev.name.c_str(), ev.payload.c_str(), ev.time);
                }
            }
        }
        ImGui::PopID();
    }

    ImGui::End();
}

} // namespace Editor
} // namespace OpenMeta

