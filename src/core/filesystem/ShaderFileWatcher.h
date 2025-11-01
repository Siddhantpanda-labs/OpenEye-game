#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <chrono>

namespace OpenEye {
namespace Core {

// Watches and copies shader files from assets/shaders/ to debug/assets/shaders/
class ShaderFileWatcher {
public:
    // Initialize the file watcher
    // sourceDir: source directory (e.g., "assets/shaders")
    // destDir: destination directory (e.g., "debug/assets/shaders")
    static bool Initialize(const std::string& sourceDir = "assets/shaders",
                          const std::string& destDir = "debug/assets/shaders");
    
    // Sync all shader files (copy new/modified files)
    // Returns number of files copied/updated
    static size_t SyncShaders();
    
    // Update - checks for changes and copies if needed (throttled)
    // Call this periodically (e.g., every second) in your main loop
    // Returns number of files copied/updated
    static size_t Update(float deltaTime = 0.0f);
    
    // Check if a file needs to be copied (newer or doesn't exist)
    static bool NeedsCopy(const std::filesystem::path& source, const std::filesystem::path& dest);
    
    // Copy a single shader file
    static bool CopyShaderFile(const std::filesystem::path& source, const std::filesystem::path& dest);
    
    // Get all shader files from source directory
    static std::vector<std::filesystem::path> GetShaderFiles();
    
    // Shutdown (cleanup if needed)
    static void Shutdown();

private:
    static std::string s_sourceDir;
    static std::string s_destDir;
    static bool s_initialized;
    static float s_checkInterval;        // Time between checks (in seconds)
    static float s_timeSinceLastCheck;   // Accumulated time since last check
    
    // Shader file extensions to watch
    static constexpr const char* s_shaderExtensions[] = { ".vert", ".frag", ".geom", ".comp", ".tess", ".tesc", ".tese" };
    static bool IsShaderFile(const std::filesystem::path& path);
};

} // namespace Core
} // namespace OpenEye

