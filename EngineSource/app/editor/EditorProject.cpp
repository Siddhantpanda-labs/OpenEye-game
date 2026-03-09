#include "EditorProject.h"
#include "api/platform/All.h"
#include <json.hpp>
#include <fstream>
#include <algorithm>
#include <cstdio>

namespace openeye::editor {

namespace fs = eyelib::api::platform::filesystem;

static bool ParseProjectDescriptor(const std::string& openeyePath, std::string& outName,
                                   std::string& outAssets, std::string& outCache) {
    std::ifstream in(openeyePath);
    if (!in.is_open())
        return false;
    nlohmann::json j;
    try {
        in >> j;
    } catch (...) {
        return false;
    }
    if (!j.is_object())
        return false;
    if (j.contains("name") && j["name"].is_string())
        outName = j["name"].get<std::string>();
    else
        outName = "Project";
    outAssets = "Assets";
    if (j.contains("assets") && j["assets"].is_string())
        outAssets = j["assets"].get<std::string>();
    outCache = "Cache";
    if (j.contains("cache") && j["cache"].is_string())
        outCache = j["cache"].get<std::string>();
    return true;
}

bool OpenProject(EditorContext& ctx, const std::string& projectFolderPath) {
    std::string absolute = fs::GetAbsolutePath(projectFolderPath);
    if (absolute.empty() || !fs::FileExists(absolute))
        return false;

    auto entries = fs::ListDirectory(absolute);
    std::string openeyePath;
    for (const auto& e : entries) {
        if (fs::GetExtension(e) == ".openeye") {
            openeyePath = fs::NormalizePath(absolute + "/" + e);
            break;
        }
    }
    if (openeyePath.empty())
        return false;

    std::string name, assetsRel, cacheRel;
    if (!ParseProjectDescriptor(openeyePath, name, assetsRel, cacheRel))
        return false;

    ctx.projectPath = absolute;
    ctx.projectName = name;
    ctx.projectAssetsRoot = fs::NormalizePath(absolute + "/" + assetsRel);
    ctx.projectCacheRoot = fs::NormalizePath(absolute + "/" + cacheRel);
    ctx.assetRoots = { ctx.projectAssetsRoot };
    return true;
}

void CloseProject(EditorContext& ctx, const std::string& engineAssetsPath) {
    ctx.projectPath.clear();
    ctx.projectName.clear();
    ctx.projectAssetsRoot.clear();
    ctx.projectCacheRoot.clear();
    ctx.assetRoots = { engineAssetsPath };
}

} // namespace openeye::editor
