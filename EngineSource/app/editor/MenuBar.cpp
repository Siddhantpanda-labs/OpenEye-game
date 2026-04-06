#include "MenuBar.h"
#include "EditorContext.h"
#include "EditorFolderPicker.h"
#include "EditorProject.h"
#include "EditorSceneBuilder.h"
#include "ScenePipelineMode.h"
#include "GameRuntime.h"
#include "api/platform/All.h"
#include "api/core/All.h"
#include "api/render/RenderDevice.h"
#include "api/render/RenderBackend.h"

#include <imgui.h>
#include <cstdio>
#include <string>

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

static void ShowCreateProjectModal(EditorContext& ctx) {
    if (ctx.activeModal != EditorContext::Modal::CreateProject) return;
    ImGui::OpenPopup("Create Project");
    ctx.activeModal = EditorContext::Modal::None;
}

static void RunCreateProjectModal(EditorContext& ctx) {
    if (!ImGui::BeginPopupModal("Create Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::Text("Project name (will be created in Projects folder):");
    ImGui::SetNextItemWidth(280.0f);
    ImGui::InputText("##projectname", ctx.projectNameBuffer, sizeof(ctx.projectNameBuffer));
    if (ImGui::Button("Create", ImVec2(120, 0))) {
        std::string path = openeye::editor::CreateProject(ctx.projectNameBuffer);
        if (!path.empty()) {
            if (openeye::editor::OpenProject(ctx, path))
                std::snprintf(ctx.projectPathBuffer, sizeof(ctx.projectPathBuffer), "%s", ctx.projectPath.c_str());
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
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
                RequestApplyScenePipelineModeToEditorPipelines(ctx);
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
    if (ctx.activeModal == EditorContext::Modal::CreateProject) ShowCreateProjectModal(ctx);

    RunOpenModal(ctx);
    RunSaveAsModal(ctx);
    RunCreateProjectModal(ctx);

    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open Project...")) {
            std::string picked = openeye::editor::ShowFolderPickerDialog();
            if (!picked.empty() && openeye::editor::OpenProject(ctx, picked)) {
                std::snprintf(ctx.projectPathBuffer, sizeof(ctx.projectPathBuffer), "%s", ctx.projectPath.c_str());
            }
        }
        if (ImGui::MenuItem("Create Project...")) {
            ctx.activeModal = EditorContext::Modal::CreateProject;
            std::snprintf(ctx.projectNameBuffer, sizeof(ctx.projectNameBuffer), "NewProject");
        }
        if (ImGui::MenuItem("Close Project", nullptr, false, !ctx.projectPath.empty())) {
            openeye::editor::CloseProject(ctx, ctx.engineAssetsPath);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Open...")) {
            ctx.activeModal = EditorContext::Modal::Open;
            if (ctx.scenePath.empty()) {
                if (!ctx.projectAssetsRoot.empty())
                    std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "%s/scenes/main.json", ctx.projectAssetsRoot.c_str());
                else
                    std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "scenes/main.json");
            } else {
                std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "%s", ctx.scenePath.c_str());
            }
        }
        if (ImGui::MenuItem("Save")) {
            if (ctx.scenePath.empty()) {
                ctx.activeModal = EditorContext::Modal::SaveAs;
                if (!ctx.projectAssetsRoot.empty())
                    std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "%s/scenes/untitled.json", ctx.projectAssetsRoot.c_str());
                else
                    std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "scenes/untitled.json");
            } else {
                ctx.world.SaveScene(ctx.scenePath);
            }
        }
        if (ImGui::MenuItem("Save As...")) {
            ctx.activeModal = EditorContext::Modal::SaveAs;
            if (ctx.scenePath.empty()) {
                if (!ctx.projectAssetsRoot.empty())
                    std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "%s/scenes/untitled.json", ctx.projectAssetsRoot.c_str());
                else
                    std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "scenes/untitled.json");
            } else {
                std::snprintf(ctx.pathBuffer, sizeof(ctx.pathBuffer), "%s", ctx.scenePath.c_str());
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Game")) {
        if (ImGui::MenuItem("Play", nullptr, false, !ctx.isPlaying && ctx.window && ctx.assetRegistry)) {
            ctx.playSnapshotPath.clear();
            std::string exeDir = eyelib::api::platform::filesystem::GetDirectory(
                eyelib::api::platform::filesystem::GetExecutablePath());
            std::string snapshotPath = exeDir.empty() ? ".openeye_play_snapshot.json"
                : (exeDir + (exeDir.back() == '/' || exeDir.back() == '\\' ? "" : "/") + ".openeye_play_snapshot.json");
            if (!ctx.world.SaveScene(snapshotPath)) {
                eyelib::api::core::Log::ErrorF("Play: failed to snapshot world to %s; Play aborted.", snapshotPath.c_str());
            } else {
                ctx.playSnapshotPath = snapshotPath;
                ctx.gameRuntime = std::make_unique<openeye::player::GameRuntime>();
                eyelib::api::RenderDevice editorDevice = ctx.sceneViewPipeline ? ctx.sceneViewPipeline->GetDevice() : eyelib::api::RenderDevice();
                if (ctx.gameRuntime->Initialize(ctx.world, *ctx.assetRegistry, ctx.assetRoots,
                                               ctx.window, 256, 256, eyelib::render::RenderBackend::Vulkan, editorDevice, nullptr)) {
                    ctx.isPlaying = true;
                } else {
                    ctx.gameRuntime.reset();
                    (void)eyelib::api::platform::filesystem::DeleteFile(ctx.playSnapshotPath);
                    ctx.playSnapshotPath.clear();
                }
            }
        }
        if (ImGui::MenuItem("Stop", nullptr, false, ctx.isPlaying)) {
            ctx.gameRuntimeStopRequested = true;
            ctx.isPlaying = false;
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

} // namespace openeye::editor
