#include "AssetPanel.h"
#include "EditorContext.h"
#include "EditorSceneBuilder.h"
#include "api/platform/All.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>

namespace openeye::editor::panels {

namespace fs = eyelib::api::platform::filesystem;

static bool IsSceneFile(const std::string& path) {
    std::string ext = fs::GetExtension(path);
    return ext == ".json" || ext == ".scene";
}

/** Minimal empty scene JSON (version 1; loader accepts this and keeps state empty). */
static const char* GetEmptySceneJson() {
    return "{\"version\":1}";
}

/** Close the current window. Docked panels are child windows and must use EndChild(). */
static void EndAssetBrowserWindow() {
    if (ImGui::IsWindowDocked())
        ImGui::EndChild();
    else
        ImGui::End();
}

// Panel state (current folder; context menu targets)
static std::string s_currentPath;
static std::string s_contextFolderPath;  // folder we right-clicked (for Create)
static std::string s_contextItemPath;    // file/folder we right-clicked (for Rename/Delete)
static char s_renameBuffer[256] = "";
static bool s_showRenameModal = false;
static bool s_showDeleteConfirm = false;
static bool s_deleteIsDirectory = false;

static void ResetCurrentPath(const std::string& projectRoot) {
    s_currentPath = projectRoot;
}

static std::string GetParentPath(const std::string& path) {
    std::string dir = fs::GetDirectory(path);
    if (dir.empty() || dir == path) return path;
    return dir;
}

static bool CreateEmptySceneFile(const std::string& folderPath) {
    std::string path = fs::NormalizePath(folderPath + "/NewScene.json");
    int suffix = 0;
    while (fs::FileExists(path)) {
        suffix++;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "/NewScene%d.json", suffix);
        path = fs::NormalizePath(folderPath + buf);
    }
    const char* content = GetEmptySceneJson();
    return fs::WriteFile(path, content, std::strlen(content));
}

static bool CreateNewFolder(const std::string& parentPath) {
    std::string path = fs::NormalizePath(parentPath + "/NewFolder");
    int suffix = 0;
    while (fs::FileExists(path)) {
        suffix++;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "/NewFolder%d", suffix);
        path = fs::NormalizePath(parentPath + buf);
    }
    return fs::CreateDirectory(path);
}

static void DrawCard(EditorContext* ctx, const std::string& fullPath, const std::string& name, bool isDir) {
    const float cardW = 80.0f;
    const float cardH = 60.0f;

    ImGui::PushID(fullPath.c_str());
    ImGui::BeginChild(name.c_str(), ImVec2(cardW, cardH), ImGuiChildFlags_Border | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushStyleColor(ImGuiCol_Text, isDir ? ImVec4(0.9f, 0.85f, 0.4f, 1.0f) : ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
    ImGui::TextUnformatted(isDir ? "[Dir]" : "[File]");
    ImGui::PopStyleColor();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
    ImGui::TextWrapped("%s", name.c_str());
    ImGui::EndChild();

    if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonRight)) {
        s_contextItemPath = fullPath;
        if (isDir) {
            if (ImGui::BeginMenu("Create")) {
                if (ImGui::MenuItem("Scene file")) CreateEmptySceneFile(fullPath);
                if (ImGui::MenuItem("Folder")) CreateNewFolder(fullPath);
                ImGui::EndMenu();
            }
            ImGui::Separator();
        }
        if (ImGui::MenuItem("Rename")) {
            std::string fname = fs::GetFilename(fullPath);
            std::snprintf(s_renameBuffer, sizeof(s_renameBuffer), "%s", fname.c_str());
            s_showRenameModal = true;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::MenuItem("Delete")) {
            s_showDeleteConfirm = true;
            s_deleteIsDirectory = isDir;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    bool doubleClick = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0);
    if (doubleClick) {
        if (isDir) {
            s_currentPath = fullPath;
        } else if (ctx && ctx->assetRegistry && IsSceneFile(fullPath)) {
            if (ctx->world.LoadScene(fullPath)) {
                ctx->scenePath = fullPath;
                EditorSceneBuilder::BindSceneAssets(ctx->world, ctx->assetRoots, *ctx->assetRegistry);
            }
        }
    }
    ImGui::PopID();
}

static void RunRenameModal() {
    if (!s_showRenameModal || s_contextItemPath.empty()) return;
    ImGui::OpenPopup("Rename");
    s_showRenameModal = false;
}

static void RunRenamePopup() {
    if (!ImGui::BeginPopupModal("Rename", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::Text("New name:");
    ImGui::SetNextItemWidth(280.0f);
    ImGui::InputText("##rename", s_renameBuffer, sizeof(s_renameBuffer));
    if (ImGui::Button("OK", ImVec2(80, 0))) {
        std::string parent = fs::GetDirectory(s_contextItemPath);
        std::string newPath = fs::NormalizePath(parent + "/" + s_renameBuffer);
        if (!newPath.empty() && newPath != s_contextItemPath && fs::RenamePath(s_contextItemPath, newPath)) {
            if (s_currentPath == s_contextItemPath)
                s_currentPath = newPath;
            else if (s_currentPath.size() > s_contextItemPath.size()) {
                size_t len = s_contextItemPath.size();
                if ((s_currentPath[len] == '/' || s_currentPath[len] == '\\') && s_currentPath.compare(0, len, s_contextItemPath) == 0)
                    s_currentPath = newPath + s_currentPath.substr(len);
            }
            s_contextItemPath.clear();
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0))) {
        s_contextItemPath.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

static void RunDeleteConfirmPopup() {
    if (!ImGui::BeginPopupModal("Delete?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::Text("Delete \"%s\"?", fs::GetFilename(s_contextItemPath).c_str());
    ImGui::TextUnformatted(s_deleteIsDirectory ? "This folder and all its contents will be deleted." : "This file will be deleted.");
    if (ImGui::Button("Delete", ImVec2(80, 0))) {
        bool ok = s_deleteIsDirectory ? fs::DeleteDirectory(s_contextItemPath) : fs::DeleteFile(s_contextItemPath);
        if (ok && s_currentPath == s_contextItemPath)
            s_currentPath = GetParentPath(s_currentPath);
        else if (ok && s_currentPath.size() > s_contextItemPath.size()) {
            const char* p = s_currentPath.c_str() + s_contextItemPath.size();
            if (*p == '/' || *p == '\\')
                s_currentPath = GetParentPath(s_contextItemPath);
        }
        s_contextItemPath.clear();
        s_showDeleteConfirm = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0))) {
        s_contextItemPath.clear();
        s_showDeleteConfirm = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void ShowAssetPanel(EditorContext* ctx) {
    RunRenameModal();
    RunRenamePopup();
    if (s_showDeleteConfirm) {
        ImGui::OpenPopup("Delete?");
        s_showDeleteConfirm = false;
    }
    RunDeleteConfirmPopup();

    if (ImGui::Begin("Asset Browser", nullptr, ImGuiWindowFlags_None)) {
        if (!ctx) {
            ImGui::TextUnformatted("No context.");
        } else if (ctx->projectPath.empty()) {
            ImGui::TextUnformatted("No project open.");
            ImGui::TextUnformatted("Use File → Open Project... or run with --project <path>.");
        } else if (ctx->projectAssetsRoot.empty()) {
            ImGui::TextUnformatted("Assets path not set.");
        } else {
            if (!fs::FileExists(ctx->projectAssetsRoot))
                fs::CreateDirectory(ctx->projectAssetsRoot);

            if (s_currentPath.empty() || s_currentPath.find(ctx->projectPath) != 0)
                ResetCurrentPath(ctx->projectPath);

            const float cardW = 80.0f;
            const float cardH = 60.0f;
            const float pad = 6.0f;
            int columns = (int)(ImGui::GetContentRegionAvail().x / (cardW + pad));
            if (columns < 1) columns = 1;

            if (s_currentPath != ctx->projectPath) {
                if (ImGui::Button("< Back")) {
                    s_currentPath = GetParentPath(s_currentPath);
                    if (s_currentPath.size() < ctx->projectPath.size() || s_currentPath.compare(0, ctx->projectPath.size(), ctx->projectPath) != 0)
                        s_currentPath = ctx->projectPath;
                }
                ImGui::Separator();
            }

            auto entries = fs::ListDirectory(s_currentPath);
            std::vector<std::string> dirs, files;
            for (const auto& e : entries) {
                std::string full = fs::NormalizePath(s_currentPath + "/" + e);
                if (fs::IsDirectory(full))
                    dirs.push_back(e);
                else
                    files.push_back(e);
            }
            std::sort(dirs.begin(), dirs.end());
            std::sort(files.begin(), files.end());

            int col = 0;
            for (const auto& e : dirs) {
                std::string fullPath = fs::NormalizePath(s_currentPath + "/" + e);
                if (col > 0) ImGui::SameLine(0, pad);
                DrawCard(ctx, fullPath, e, true);
                col++;
                if (col >= columns) { col = 0; }
            }
            for (const auto& e : files) {
                std::string fullPath = fs::NormalizePath(s_currentPath + "/" + e);
                if (col > 0) ImGui::SameLine(0, pad);
                DrawCard(ctx, fullPath, e, false);
                col++;
                if (col >= columns) { col = 0; }
            }

            if (dirs.empty() && files.empty()) {
                ImGui::TextUnformatted("Empty folder.");
                ImGui::TextUnformatted("Right-click here to create Scene or Folder.");
            }

            if (ImGui::BeginPopupContextWindow("AssetBackgroundContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                if (ImGui::BeginMenu("Create")) {
                    if (ImGui::MenuItem("Scene file")) {
                        CreateEmptySceneFile(s_currentPath);
                    }
                    if (ImGui::MenuItem("Folder")) {
                        CreateNewFolder(s_currentPath);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }
        }
    }
    EndAssetBrowserWindow();
}

} // namespace openeye::editor::panels
