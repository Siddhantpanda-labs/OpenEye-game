#include "MenuBar.h"
#include "EditorContext.h"
#include "EditorSceneBuilder.h"

#include <imgui.h>
#include <cstdio>

namespace openeye::editor {

static void ShowOpenModal(EditorContext& ctx) {
    if (ctx.activeModal != EditorContext::Modal::Open) return;
    ImGui::OpenPopup("Open Scene");
    ctx.activeModal = EditorContext::Modal::None;  // open once
}

static void ShowSaveAsModal(EditorContext& ctx) {
    if (ctx.activeModal != EditorContext::Modal::SaveAs) return;
    ImGui::OpenPopup("Save Scene As");
    ctx.activeModal = EditorContext::Modal::None;
}

static void RunOpenModal(EditorContext& ctx) {
    if (!ImGui::BeginPopupModal("Open Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::Text("Scene path (relative or absolute):");
    ImGui::SetNextItemWidth(400.0f);
    ImGui::InputText("##openpath", ctx.pathBuffer, sizeof(ctx.pathBuffer));
        if (ImGui::Button("Open", ImVec2(120, 0))) {
            if (ctx.world.LoadScene(ctx.pathBuffer)) {
                ctx.scenePath = ctx.pathBuffer;
                if (ctx.assetRegistry) {
                    EditorSceneBuilder::BindSceneAssets(ctx.world, ctx.assetRoots, *ctx.assetRegistry);
                }
                ImGui::CloseCurrentPopup();
            }
        }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

static void RunSaveAsModal(EditorContext& ctx) {
    if (!ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::Text("Scene path (relative or absolute):");
    ImGui::SetNextItemWidth(400.0f);
    ImGui::InputText("##saveaspath", ctx.pathBuffer, sizeof(ctx.pathBuffer));
    if (ImGui::Button("Save", ImVec2(120, 0))) {
        if (ctx.world.SaveScene(ctx.pathBuffer)) {
            ctx.scenePath = ctx.pathBuffer;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void ShowMenuBar(EditorContext& ctx) {
    if (ctx.activeModal == EditorContext::Modal::Open) ShowOpenModal(ctx);
    if (ctx.activeModal == EditorContext::Modal::SaveAs) ShowSaveAsModal(ctx);

    RunOpenModal(ctx);
    RunSaveAsModal(ctx);

    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open...")) {
            ctx.activeModal = EditorContext::Modal::Open;
            if (ctx.scenePath.empty()) {
                std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "scenes/main.json");
            } else {
                std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "%s", ctx.scenePath.c_str());
            }
        }
        if (ImGui::MenuItem("Save")) {
            if (ctx.scenePath.empty()) {
                ctx.activeModal = EditorContext::Modal::SaveAs;
                std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "scenes/untitled.json");
            } else {
                ctx.world.SaveScene(ctx.scenePath);
            }
        }
        if (ImGui::MenuItem("Save As...")) {
            ctx.activeModal = EditorContext::Modal::SaveAs;
            if (ctx.scenePath.empty()) {
                std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "scenes/untitled.json");
            } else {
                std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "%s", ctx.scenePath.c_str());
            }
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

} // namespace openeye::editor
