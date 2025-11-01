#include "ShaderFileWatcher.h"
#include "core/log/Log.h"
#include <algorithm>
#include <fstream>
#include <cctype>
#include <chrono>

namespace OpenEye {
namespace Core {

std::string ShaderFileWatcher::s_sourceDir = "assets/shaders";
std::string ShaderFileWatcher::s_destDir = "debug/assets/shaders";
bool ShaderFileWatcher::s_initialized = false;

constexpr const char* ShaderFileWatcher::s_shaderExtensions[];

bool ShaderFileWatcher::Initialize(const std::string& sourceDir, const std::string& destDir) {
    if (s_initialized) {
        return true;
    }
    
    s_sourceDir = sourceDir;
    s_destDir = destDir;
    
    // Check if source directory exists
    if (!std::filesystem::exists(s_sourceDir)) {
        OE_WARN_F("ShaderFileWatcher: Source directory does not exist: %s", s_sourceDir.c_str());
        return false;
    }
    
    // Create destination directory if it doesn't exist
    if (!std::filesystem::exists(s_destDir)) {
        try {
            std::filesystem::create_directories(s_destDir);
            OE_INFO_F("ShaderFileWatcher: Created destination directory: %s", s_destDir.c_str());
        } catch (const std::filesystem::filesystem_error& e) {
            OE_ERROR_F("ShaderFileWatcher: Failed to create destination directory: %s - %s", 
                       s_destDir.c_str(), e.what());
            return false;
        }
    }
    
    s_initialized = true;
    OE_INFO_F("ShaderFileWatcher: Initialized (source: %s, dest: %s)", s_sourceDir.c_str(), s_destDir.c_str());
    
    // Sync on initialization
    size_t copied = SyncShaders();
    if (copied > 0) {
        OE_INFO_F("ShaderFileWatcher: Copied %zu shader file(s) on initialization", copied);
    }
    
    return true;
}

bool ShaderFileWatcher::IsShaderFile(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    for (const char* shaderExt : s_shaderExtensions) {
        if (ext == shaderExt) {
            return true;
        }
    }
    return false;
}

std::vector<std::filesystem::path> ShaderFileWatcher::GetShaderFiles() {
    std::vector<std::filesystem::path> shaderFiles;
    
    if (!s_initialized) {
        return shaderFiles;
    }
    
    // Find project root by going up from executable directory
    std::filesystem::path currentPath = std::filesystem::current_path();
    std::filesystem::path projectRoot;
    
    // Prefer the top-most ancestor that contains assets/shaders (project root),
    // not the nearest one (which may be the runtime folder next to the exe)
    for (int i = 0; i < 8 && currentPath != currentPath.root_path(); i++) {
        std::filesystem::path testPath = currentPath / s_sourceDir;
        if (std::filesystem::exists(testPath) && std::filesystem::is_directory(testPath)) {
            projectRoot = currentPath; // keep updating to get the highest one
        }
        currentPath = currentPath.parent_path();
    }
    
    if (projectRoot.empty()) {
        OE_WARN_F("ShaderFileWatcher: Could not find project root with %s directory", s_sourceDir.c_str());
        return shaderFiles;
    }
    
    std::filesystem::path sourceDirPath = projectRoot / s_sourceDir;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(sourceDirPath)) {
            if (entry.is_regular_file() && IsShaderFile(entry.path())) {
                shaderFiles.push_back(entry.path());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        OE_ERROR_F("ShaderFileWatcher: Error scanning directory: %s", e.what());
    }
    
    return shaderFiles;
}

bool ShaderFileWatcher::NeedsCopy(const std::filesystem::path& source, const std::filesystem::path& dest) {
    if (!std::filesystem::exists(source)) {
        return false;
    }
    
    if (!std::filesystem::exists(dest)) {
        return true;
    }
    
    try {
        auto sourceTime = std::filesystem::last_write_time(source);
        auto destTime = std::filesystem::last_write_time(dest);
        
        // Source is newer if sourceTime > destTime
        if (sourceTime > destTime) {
            return true;
        }
        
        // If equal or dest is newer, no copy needed
        return false;
    } catch (...) {
        // On error, copy to be safe
        return true;
    }
}

bool ShaderFileWatcher::CopyShaderFile(const std::filesystem::path& source, const std::filesystem::path& dest) {
    try {
        if (dest.has_parent_path()) {
            std::filesystem::create_directories(dest.parent_path());
        }
        
        std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing);
        OE_INFO_F("ShaderFileWatcher: Copied %s", source.filename().string().c_str());
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        OE_ERROR_F("ShaderFileWatcher: Failed to copy %s: %s", 
                   source.filename().string().c_str(), e.what());
        return false;
    }
}

size_t ShaderFileWatcher::SyncShaders() {
    if (!s_initialized) {
        OE_WARN("ShaderFileWatcher: Not initialized, call Initialize() first");
        return 0;
    }
    
    // Find project root
    std::filesystem::path currentPath = std::filesystem::current_path();
    std::filesystem::path projectRoot;
    
    // Prefer the top-most ancestor that contains assets/shaders
    for (int i = 0; i < 8 && currentPath != currentPath.root_path(); i++) {
        std::filesystem::path testPath = currentPath / s_sourceDir;
        if (std::filesystem::exists(testPath) && std::filesystem::is_directory(testPath)) {
            projectRoot = currentPath; // keep updating to get the highest one
        }
        currentPath = currentPath.parent_path();
    }
    
    if (projectRoot.empty()) {
        OE_ERROR("ShaderFileWatcher: Could not find project root");
        return 0;
    }
    
    std::filesystem::path sourceDir = projectRoot / s_sourceDir;
    std::filesystem::path destDir = std::filesystem::absolute(s_destDir);
    
    if (!std::filesystem::exists(sourceDir)) {
        OE_WARN_F("ShaderFileWatcher: Source directory does not exist: %s", sourceDir.string().c_str());
        return 0;
    }
    
    size_t copiedCount = 0;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(sourceDir)) {
            if (!entry.is_regular_file() || !IsShaderFile(entry.path())) {
                continue;
            }
            
            std::filesystem::path sourceFile = entry.path();
            std::filesystem::path destFile = destDir / sourceFile.filename();
            
            // Skip if same file
            try {
                if (std::filesystem::equivalent(sourceFile, destFile)) {
                    continue;
                }
            } catch (...) {
                // If equivalent check fails, continue (might be different)
            }
            
            if (NeedsCopy(sourceFile, destFile)) {
                if (CopyShaderFile(sourceFile, destFile)) {
                    copiedCount++;
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        OE_ERROR_F("ShaderFileWatcher: Error during sync: %s", e.what());
    }
    
    return copiedCount;
}

void ShaderFileWatcher::Shutdown() {
    s_initialized = false;
}

} // namespace Core
} // namespace OpenEye

