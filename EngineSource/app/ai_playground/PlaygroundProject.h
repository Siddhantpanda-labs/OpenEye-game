#pragma once

#include <string>
#include <vector>

namespace openeye::ai_playground {

/** Walk up from the executable until a directory contains both EngineSource and Projects. */
std::string FindOpenEyeWorkspaceRoot();

constexpr const char* kPlaygroundParentDir = "Projects/AI_Playground";
/** Manifest on disk: <project_folder_name>.openeye (same name as the AI_Playground/<name> directory). */
constexpr const char* kDefaultSceneRelative = "Scene/main.scene";
/** Under engine assets root: <assets>/AI_Playground/default.scene (optional packaged template). */
constexpr const char* kPackagedTemplateRelative = "AI_Playground/default.scene";

/** Minimal scene: Hybrid pipeline + horizontal-cross evening sky (alias expands to Skybox/SkyMap/Cubemap_Sky_512x512.png). */
constexpr const char* kMinimalEmptySceneJson =
    "{\"version\":1,\"renderPipelineMode\":\"Hybrid\",\"skybox\":\"evening\"}";

struct PlaygroundManifest {
    std::string name;
    /** Relative to project root, POSIX-style for JSON. */
    std::string sceneRelative = kDefaultSceneRelative;
    int version = 1;
};

/** Sanitize user-chosen folder name: [A-Za-z0-9_-]+; empty if invalid. */
std::string SanitizePlaygroundName(const std::string& raw);

/** Absolute path: <workspace>/Projects/AI_Playground/<name> */
std::string PlaygroundProjectRoot(const std::string& workspaceRoot, const std::string& sanitizedName);

/** Subdirectory names only under Projects/AI_Playground (directories with <name>.openeye). */
std::vector<std::string> ListPlaygroundProjects(const std::string& workspaceRoot);

/**
 * Create project tree: Assets/, Scene/, AI/, Scene/main.scene, and <name>.openeye (manifest).
 * If \a usePackagedTemplate, copy from engine assets kPackagedTemplateRelative; else write kMinimalEmptySceneJson.
 * \param engineAssetsRootAbs Normalized absolute path to engine .../assets (for template lookup).
 * \return Error message on failure; empty on success.
 */
std::string CreatePlaygroundProject(const std::string& workspaceRoot, const std::string& sanitizedName,
                                    bool usePackagedTemplate, const std::string& engineAssetsRootAbs);

/**
 * Read manifest from project root. \return Error message on failure.
 */
std::string ReadPlaygroundManifest(const std::string& projectRootAbs, PlaygroundManifest& out);

/**
 * Write manifest (after successful create or future edits).
 */
bool WritePlaygroundManifest(const std::string& projectRootAbs, const PlaygroundManifest& m);

/** Absolute path to active scene file. */
std::string ResolveScenePath(const std::string& projectRootAbs, const PlaygroundManifest& m);

} // namespace openeye::ai_playground
