#include "PlaygroundProject.h"

#include "api/platform/All.h"

#include <json.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace openeye::ai_playground {

namespace fs = eyelib::api::platform::filesystem;

std::string FindOpenEyeWorkspaceRoot() {
    std::string exeDir = fs::GetDirectory(fs::GetExecutablePath());
    if (exeDir.empty()) {
        return {};
    }
    for (int up = 0; up < 12; ++up) {
        std::string candidate = exeDir;
        for (int i = 0; i < up; ++i) {
            candidate = fs::NormalizePath(candidate + "/..");
        }
        if (candidate.empty()) {
            continue;
        }
        const std::string engineSource = fs::NormalizePath(candidate + "/EngineSource");
        const std::string projects = fs::NormalizePath(candidate + "/Projects");
        if (fs::IsDirectory(engineSource) && fs::IsDirectory(projects)) {
            return fs::NormalizePath(candidate);
        }
    }
    return {};
}

std::string SanitizePlaygroundName(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            out += c;
        }
    }
    if (out.empty() || out.size() > 64) {
        return {};
    }
    return out;
}

std::string PlaygroundProjectRoot(const std::string& workspaceRoot, const std::string& sanitizedName) {
    if (workspaceRoot.empty() || sanitizedName.empty()) {
        return {};
    }
    return fs::NormalizePath(workspaceRoot + "/" + kPlaygroundParentDir + "/" + sanitizedName);
}

static std::string ProjectFolderBasename(const std::string& projectRootAbs) {
    std::string n = fs::NormalizePath(projectRootAbs);
    while (!n.empty() && (n.back() == '/' || n.back() == '\\')) {
        n.pop_back();
    }
    if (n.empty()) {
        return {};
    }
    const size_t pos = n.find_last_of("/\\");
    if (pos == std::string::npos) {
        return n;
    }
    return n.substr(pos + 1);
}

/** <projectRoot>/<folderBasename>.openeye — folder name must match manifest `name` for new projects. */
static std::string ManifestPath(const std::string& projectRootAbs) {
    const std::string base = ProjectFolderBasename(projectRootAbs);
    if (base.empty()) {
        return {};
    }
    return fs::NormalizePath(projectRootAbs + "/" + base + ".openeye");
}

std::vector<std::string> ListPlaygroundProjects(const std::string& workspaceRoot) {
    std::vector<std::string> names;
    if (workspaceRoot.empty()) {
        return names;
    }
    const std::string parent = fs::NormalizePath(workspaceRoot + "/" + kPlaygroundParentDir);
    if (!fs::FileExists(parent)) {
        return names;
    }
    for (const std::string& entry : fs::ListDirectory(parent)) {
        if (entry == "." || entry == "..") {
            continue;
        }
        const std::string sub = fs::NormalizePath(parent + "/" + entry);
        if (!fs::IsDirectory(sub)) {
            continue;
        }
        if (fs::FileExists(ManifestPath(sub))) {
            names.push_back(entry);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

static bool CopyFileBytes(const std::string& from, const std::string& to, std::string& error) {
    std::vector<uint8_t> bytes = fs::ReadFile(from);
    if (bytes.empty()) {
        error = "Empty or unreadable: " + from;
        return false;
    }
    if (!fs::WriteFile(to, bytes)) {
        error = "Cannot write: " + to;
        return false;
    }
    return true;
}

static std::string PackagedTemplateAbsolutePath(const std::string& engineAssetsRootAbs) {
    if (engineAssetsRootAbs.empty()) {
        return {};
    }
    const std::string p = fs::NormalizePath(engineAssetsRootAbs + "/" + kPackagedTemplateRelative);
    if (fs::FileExists(p)) {
        return p;
    }
    return {};
}

static std::string SerializePlaygroundManifestJson(const PlaygroundManifest& m) {
    nlohmann::json j;
    j["format"] = "openeye_ai_playground";
    j["version"] = m.version;
    j["name"] = m.name;
    j["scene"] = m.sceneRelative;
    return j.dump(2);
}

std::string CreatePlaygroundProject(const std::string& workspaceRoot, const std::string& sanitizedName,
                                    bool usePackagedTemplate, const std::string& engineAssetsRootAbs) {
    if (workspaceRoot.empty()) {
        return "Workspace root not found (open the app from the OpenEye tree).";
    }
    const std::string name = SanitizePlaygroundName(sanitizedName);
    if (name.empty()) {
        return "Invalid name (use letters, numbers, underscore, hyphen; max 64).";
    }
    const std::string root = PlaygroundProjectRoot(workspaceRoot, name);
    if (root.empty()) {
        return "Invalid project path.";
    }
    if (fs::IsDirectory(root) || fs::FileExists(ManifestPath(root))) {
        return "A playground with that name already exists.";
    }

    const std::string parent = fs::NormalizePath(workspaceRoot + "/" + kPlaygroundParentDir);
    if (!fs::CreateDirectory(parent)) {
        return "Failed to create Projects/AI_Playground directory.";
    }

    const std::string assets = fs::NormalizePath(root + "/Assets");
    const std::string sceneDir = fs::NormalizePath(root + "/Scene");
    const std::string aiDir = fs::NormalizePath(root + "/AI");
    if (!fs::CreateDirectory(assets) || !fs::CreateDirectory(sceneDir) || !fs::CreateDirectory(aiDir)) {
        return "Failed to create project directories.";
    }

    const std::string sceneDest = fs::NormalizePath(sceneDir + "/main.scene");
    if (usePackagedTemplate) {
        const std::string tmpl = PackagedTemplateAbsolutePath(engineAssetsRootAbs);
        if (tmpl.empty()) {
            return std::string("Packaged scene template not found (expected ") + kPackagedTemplateRelative
                + " under engine assets).";
        }
        std::string copyErr;
        if (!CopyFileBytes(tmpl, sceneDest, copyErr)) {
            return copyErr;
        }
    } else {
        if (!fs::WriteFile(sceneDest, kMinimalEmptySceneJson, std::strlen(kMinimalEmptySceneJson))) {
            return "Failed to write Scene/main.scene.";
        }
    }

    PlaygroundManifest m;
    m.name = name;
    m.sceneRelative = kDefaultSceneRelative;
    m.version = 1;
    const std::string manifestJson = SerializePlaygroundManifestJson(m);
    const std::string manifestPath = ManifestPath(root);
    if (!fs::WriteFile(manifestPath, manifestJson.data(), manifestJson.size())) {
        return std::string("Failed to write ") + name + ".openeye.";
    }
    return {};
}

std::string ReadPlaygroundManifest(const std::string& projectRootAbs, PlaygroundManifest& out) {
    const std::string path = ManifestPath(projectRootAbs);
    std::vector<uint8_t> raw = fs::ReadFile(path);
    if (raw.empty()) {
        const std::string base = ProjectFolderBasename(projectRootAbs);
        return std::string("Cannot read ") + (base.empty() ? std::string("manifest") : base + ".openeye");
    }
    const std::string bytes(reinterpret_cast<const char*>(raw.data()), raw.size());
    try {
        nlohmann::json j = nlohmann::json::parse(bytes);
        if (!j.contains("format") || j["format"].get<std::string>() != "openeye_ai_playground") {
            return "Invalid playground manifest format.";
        }
        out.version = j.value("version", 1);
        out.name = j.value("name", std::string{});
        out.sceneRelative = j.value("scene", std::string(kDefaultSceneRelative));
        if (out.sceneRelative.empty()) {
            out.sceneRelative = kDefaultSceneRelative;
        }
    } catch (...) {
        return "Failed to parse playground manifest JSON.";
    }
    return {};
}

bool WritePlaygroundManifest(const std::string& projectRootAbs, const PlaygroundManifest& m) {
    const std::string dumped = SerializePlaygroundManifestJson(m);
    const std::string path = ManifestPath(projectRootAbs);
    return fs::WriteFile(path, dumped.data(), dumped.size());
}

std::string ResolveScenePath(const std::string& projectRootAbs, const PlaygroundManifest& m) {
    if (projectRootAbs.empty()) {
        return {};
    }
    std::string rel = m.sceneRelative;
    for (char& c : rel) {
        if (c == '\\') {
            c = '/';
        }
    }
    return fs::NormalizePath(projectRootAbs + "/" + rel);
}

} // namespace openeye::ai_playground
