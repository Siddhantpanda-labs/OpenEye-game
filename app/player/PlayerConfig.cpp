#include "PlayerConfig.h"

#include "api/platform/All.h"
#include "eyelib/render/RenderBackend.h"

#include <json.hpp>

#include <cstdlib>
#include <iostream>

namespace openeye::player {

void PrintUsage(const char* exeName) {
    std::cout
        << "Usage: " << exeName << " [options]\n"
        << "Options:\n"
        << "  --config <path>    Optional config JSON file\n"
        << "  --profile <name>   Runtime profile: dev | shipped\n"
        << "  --scene <path>     Path to scene JSON (default: scenes/main.json)\n"
        << "  --assets <path>    Asset root folder (can be repeated)\n"
        << "  --log <path>       Log file path (default: OpenEyePlayer.log)\n"
        << "  --width <number>   Window width (default: 1280)\n"
        << "  --height <number>  Window height (default: 720)\n"
        << "  --title <string>   Window title (default: OpenEye Player)\n"
        << "  --novsync          Disable vsync\n"
        << "  --renderer <name>  Render backend: opengl | diligent (default: opengl)\n"
        << "  --headless         Run without creating a window\n"
        << "  --fixed <seconds>  Fixed timestep override\n"
        << "  --time-scale <n>   Time scale multiplier (default: 1.0)\n"
        << "  --time-mode <m>    realtime | fixed | replay\n"
        << "  --replay-delta <s> Replay delta (seconds) for replay mode\n"
        << "  --max-substeps <n> Max fixed substeps (default: 8)\n"
        << "  --paused           Start paused\n"
        << "  --step             Start paused and run one step\n"
        << "  --stats            Log fps/entity stats every second\n"
        << "  --strict-assets    Fail if any referenced asset is missing\n"
        << "  --overlay          Show a lightweight on-screen overlay\n"
        << "  --help             Show this help message\n";
}

static bool ParseInt(const char* value, int& outValue) {
    if (!value) {
        return false;
    }
    char* end = nullptr;
    long parsed = std::strtol(value, &end, 10);
    if (!end || *end != '\0') {
        return false;
    }
    outValue = static_cast<int>(parsed);
    return true;
}

static bool ParseFloat(const char* value, float& outValue) {
    if (!value) {
        return false;
    }
    char* end = nullptr;
    float parsed = std::strtof(value, &end);
    if (!end || *end != '\0') {
        return false;
    }
    outValue = parsed;
    return true;
}

std::string FindConfigPath(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--config" && i + 1 < argc) {
            return argv[i + 1];
        }
    }
    return {};
}

static bool IsAbsolutePath(const std::string& path) {
    if (path.empty()) {
        return false;
    }
#ifdef _WIN32
    if (path.size() > 1 && path[1] == ':') {
        return true;
    }
#endif
    return path[0] == '/' || path[0] == '\\';
}

static std::string ResolveRelativePath(const std::string& path, const std::string& baseDir) {
    using namespace eyelib::api::platform::filesystem;
    if (path.empty()) {
        return {};
    }
    if (IsAbsolutePath(path)) {
        return NormalizePath(path);
    }
    if (baseDir.empty()) {
        return NormalizePath(path);
    }
    return NormalizePath(baseDir + "/" + path);
}

static void ApplyString(const nlohmann::json& j, const char* key, std::string& out) {
    if (j.contains(key) && j.at(key).is_string()) {
        out = j.at(key).get<std::string>();
    }
}

static void ApplyBool(const nlohmann::json& j, const char* key, bool& out, bool* wasSet = nullptr) {
    if (j.contains(key) && j.at(key).is_boolean()) {
        out = j.at(key).get<bool>();
        if (wasSet) {
            *wasSet = true;
        }
    }
}

static void ApplyInt(const nlohmann::json& j, const char* key, int& out) {
    if (j.contains(key) && j.at(key).is_number_integer()) {
        out = j.at(key).get<int>();
    }
}

static void ApplyFloat(const nlohmann::json& j, const char* key, float& out) {
    if (j.contains(key) && j.at(key).is_number()) {
        out = j.at(key).get<float>();
    }
}

static void ApplyStringArray(const nlohmann::json& j, const char* key, std::vector<std::string>& out) {
    if (!j.contains(key) || !j.at(key).is_array()) {
        return;
    }
    out.clear();
    for (const auto& item : j.at(key)) {
        if (item.is_string()) {
            out.emplace_back(item.get<std::string>());
        }
    }
}

bool LoadConfig(const std::string& path, RuntimeOptions& options, std::string& error) {
    using namespace eyelib::api::platform::filesystem;
    if (path.empty()) {
        return true;
    }
    if (!FileExists(path)) {
        error = "Config file not found: " + path;
        return false;
    }

    const auto data = ReadFile(path);
    if (data.empty()) {
        error = "Config file is empty or unreadable: " + path;
        return false;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(data.begin(), data.end());
    } catch (const std::exception& ex) {
        error = std::string("Failed to parse config JSON: ") + ex.what();
        return false;
    }

    options.configPath = path;
    const std::string baseDir = GetDirectory(path);

    ApplyString(j, "profile", options.profile);
    ApplyString(j, "scenePath", options.scenePath);
    ApplyStringArray(j, "assetRoots", options.assetRoots);
    ApplyString(j, "logPath", options.logPath);
    ApplyString(j, "title", options.title);

    ApplyInt(j, "width", options.width);
    ApplyInt(j, "height", options.height);
    ApplyInt(j, "maxSubsteps", options.maxSubsteps);

    ApplyFloat(j, "fixedTimestep", options.fixedTimestep);
    ApplyFloat(j, "timeScale", options.timeScale);
    ApplyFloat(j, "replayDelta", options.replayDelta);

    ApplyBool(j, "headless", options.headless, &options.headlessSet);
    ApplyBool(j, "vsync", options.vsync);
    ApplyBool(j, "paused", options.paused);
    ApplyBool(j, "stepOnce", options.stepOnce);
    ApplyBool(j, "stats", options.stats, &options.statsSet);
    ApplyBool(j, "strictAssets", options.strictAssets, &options.strictAssetsSet);
    ApplyBool(j, "overlay", options.overlay, &options.overlaySet);

    if (j.contains("renderer") && j.at("renderer").is_string()) {
        const std::string r = j.at("renderer").get<std::string>();
        if (r == "opengl") options.renderBackend = eyelib::render::RenderBackend::OpenGL;
        else if (r == "diligent") options.renderBackend = eyelib::render::RenderBackend::Diligent;
        else if (r == "default") options.renderBackend = eyelib::render::RenderBackend::Default;
    }

    if (j.contains("timeMode") && j.at("timeMode").is_string()) {
        const std::string mode = j.at("timeMode").get<std::string>();
        options.timeModeSet = true;
        if (mode == "realtime") {
            options.timeMode = TimeMode::Realtime;
        } else if (mode == "fixed") {
            options.timeMode = TimeMode::Fixed;
        } else if (mode == "replay") {
            options.timeMode = TimeMode::Replay;
        } else {
            error = "Invalid timeMode in config: " + mode;
            return false;
        }
    }

    options.scenePath = ResolveRelativePath(options.scenePath, baseDir);
    for (auto& root : options.assetRoots) {
        root = ResolveRelativePath(root, baseDir);
    }

    return true;
}

bool ParseArgs(int argc, char** argv, RuntimeOptions& options, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            options.showHelp = true;
            return true;
        }
        if (arg == "--profile" && i + 1 < argc) {
            options.profile = argv[++i];
            continue;
        }
        if (arg == "--config" && i + 1 < argc) {
            options.configPath = argv[++i];
            continue;
        }
        if (arg == "--scene" && i + 1 < argc) {
            options.scenePath = argv[++i];
            continue;
        }
        if (arg == "--assets" && i + 1 < argc) {
            options.assetRoots.emplace_back(argv[++i]);
            continue;
        }
        if (arg == "--log" && i + 1 < argc) {
            options.logPath = argv[++i];
            continue;
        }
        if (arg == "--width" && i + 1 < argc) {
            if (!ParseInt(argv[++i], options.width)) {
                error = "Invalid --width value.";
                return false;
            }
            continue;
        }
        if (arg == "--height" && i + 1 < argc) {
            if (!ParseInt(argv[++i], options.height)) {
                error = "Invalid --height value.";
                return false;
            }
            continue;
        }
        if (arg == "--title" && i + 1 < argc) {
            options.title = argv[++i];
            continue;
        }
        if (arg == "--fixed" && i + 1 < argc) {
            if (!ParseFloat(argv[++i], options.fixedTimestep)) {
                error = "Invalid --fixed value.";
                return false;
            }
            continue;
        }
        if (arg == "--time-scale" && i + 1 < argc) {
            if (!ParseFloat(argv[++i], options.timeScale)) {
                error = "Invalid --time-scale value.";
                return false;
            }
            continue;
        }
        if (arg == "--replay-delta" && i + 1 < argc) {
            if (!ParseFloat(argv[++i], options.replayDelta)) {
                error = "Invalid --replay-delta value.";
                return false;
            }
            continue;
        }
        if (arg == "--max-substeps" && i + 1 < argc) {
            if (!ParseInt(argv[++i], options.maxSubsteps)) {
                error = "Invalid --max-substeps value.";
                return false;
            }
            continue;
        }
        if (arg == "--time-mode" && i + 1 < argc) {
            const std::string mode = argv[++i];
            options.timeModeSet = true;
            if (mode == "realtime") {
                options.timeMode = TimeMode::Realtime;
            } else if (mode == "fixed") {
                options.timeMode = TimeMode::Fixed;
            } else if (mode == "replay") {
                options.timeMode = TimeMode::Replay;
            } else {
                error = "Invalid --time-mode value.";
                return false;
            }
            continue;
        }
        if (arg == "--novsync") {
            options.vsync = false;
            continue;
        }
        if (arg == "--headless") {
            options.headless = true;
            options.headlessSet = true;
            continue;
        }
        if (arg == "--paused") {
            options.paused = true;
            continue;
        }
        if (arg == "--step") {
            options.stepOnce = true;
            options.paused = true;
            continue;
        }
        if (arg == "--stats") {
            options.stats = true;
            options.statsSet = true;
            continue;
        }
        if (arg == "--strict-assets") {
            options.strictAssets = true;
            options.strictAssetsSet = true;
            continue;
        }
        if (arg == "--overlay") {
            options.overlay = true;
            options.overlaySet = true;
            continue;
        }
        if (arg == "--renderer" && i + 1 < argc) {
            const std::string r = argv[++i];
            if (r == "opengl") options.renderBackend = eyelib::render::RenderBackend::OpenGL;
            else if (r == "diligent") options.renderBackend = eyelib::render::RenderBackend::Diligent;
            else if (r == "default") options.renderBackend = eyelib::render::RenderBackend::Default;
            else {
                error = "Invalid --renderer value; use opengl, diligent, or default.";
                return false;
            }
            continue;
        }

        error = "Unknown or incomplete argument: " + arg;
        return false;
    }
    return true;
}

} // namespace openeye::player
