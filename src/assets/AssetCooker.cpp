#include "AssetCooker.h"
#include "core/log/Log.h"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace OpenEye {
namespace Assets {

// ============================================================================
// CookingOptions Implementation
// ============================================================================

nlohmann::json CookingOptions::ToJson() const {
    nlohmann::json json;
    json["compressMesh"] = compressMesh;
    json["optimizeMesh"] = optimizeMesh;
    json["meshOptimizationLevel"] = meshOptimizationLevel;
    json["compressTexture"] = compressTexture;
    json["textureFormat"] = textureFormat;
    json["generateMipmaps"] = generateMipmaps;
    json["maxTextureSize"] = maxTextureSize;
    json["serializeMaterial"] = serializeMaterial;
    json["embedTextures"] = embedTextures;
    json["compressAnimation"] = compressAnimation;
    json["animationTolerance"] = animationTolerance;
    json["removeRedundantKeys"] = removeRedundantKeys;
    json["verbose"] = verbose;
    json["outputDirectory"] = outputDirectory;
    return json;
}

CookingOptions CookingOptions::FromJson(const nlohmann::json& json) {
    CookingOptions options;
    if (json.contains("compressMesh")) options.compressMesh = json["compressMesh"];
    if (json.contains("optimizeMesh")) options.optimizeMesh = json["optimizeMesh"];
    if (json.contains("meshOptimizationLevel")) options.meshOptimizationLevel = json["meshOptimizationLevel"];
    if (json.contains("compressTexture")) options.compressTexture = json["compressTexture"];
    if (json.contains("textureFormat")) options.textureFormat = json["textureFormat"];
    if (json.contains("generateMipmaps")) options.generateMipmaps = json["generateMipmaps"];
    if (json.contains("maxTextureSize")) options.maxTextureSize = json["maxTextureSize"];
    if (json.contains("serializeMaterial")) options.serializeMaterial = json["serializeMaterial"];
    if (json.contains("embedTextures")) options.embedTextures = json["embedTextures"];
    if (json.contains("compressAnimation")) options.compressAnimation = json["compressAnimation"];
    if (json.contains("animationTolerance")) options.animationTolerance = json["animationTolerance"];
    if (json.contains("removeRedundantKeys")) options.removeRedundantKeys = json["removeRedundantKeys"];
    if (json.contains("verbose")) options.verbose = json["verbose"];
    if (json.contains("outputDirectory")) options.outputDirectory = json["outputDirectory"];
    return options;
}

// ============================================================================
// CookingResult Implementation
// ============================================================================

nlohmann::json CookingResult::ToJson() const {
    nlohmann::json json;
    json["success"] = success;
    json["assetGUID"] = assetGUID.ToString();
    json["inputPath"] = inputPath;
    json["outputPath"] = outputPath;
    json["errorMessage"] = errorMessage;
    json["originalSize"] = originalSize;
    json["cookedSize"] = cookedSize;
    json["compressionRatio"] = compressionRatio;
    json["assetType"] = assetType;
    
    json["dependencies"] = nlohmann::json::array();
    for (const auto& dep : dependencies) {
        json["dependencies"].push_back(dep.ToString());
    }
    
    return json;
}

// ============================================================================
// AssetCooker Implementation
// ============================================================================

AssetCooker::AssetCooker() {
    m_options = CookingOptions();
}

std::string AssetCooker::GetOutputPath(const std::string& inputPath, const std::string& extension) const {
    std::filesystem::path input(inputPath);
    std::filesystem::path outputDir(m_options.outputDirectory);
    
    // Create output directory structure matching input
    std::filesystem::path relativePath = std::filesystem::relative(input.parent_path(), std::filesystem::current_path());
    std::filesystem::path outputPath = outputDir / relativePath / (input.stem().string() + extension);
    
    // Create parent directories
    std::filesystem::create_directories(outputPath.parent_path());
    
    return outputPath.string();
}

size_t AssetCooker::GetFileSize(const std::string& filepath) const {
    try {
        return std::filesystem::file_size(filepath);
    } catch (const std::exception&) {
        return 0;
    }
}

void AssetCooker::UpdateStatistics(const CookingResult& result) {
    m_statistics.totalAssetsCooked++;
    
    if (result.success) {
        m_statistics.successfulAssets++;
        m_statistics.totalOriginalSize += result.originalSize;
        m_statistics.totalCookedSize += result.cookedSize;
        
        // Update average compression ratio
        if (m_statistics.successfulAssets > 0) {
            float totalRatio = m_statistics.totalOriginalSize > 0
                ? 1.0f - (static_cast<float>(m_statistics.totalCookedSize) / static_cast<float>(m_statistics.totalOriginalSize))
                : 0.0f;
            m_statistics.averageCompressionRatio = totalRatio;
        }
    } else {
        m_statistics.failedAssets++;
    }
}

CookingResult AssetCooker::CookAsset(const std::string& inputPath, const std::string& assetType) {
    CookingResult result;
    result.inputPath = inputPath;
    result.assetType = assetType;
    
    if (!std::filesystem::exists(inputPath)) {
        result.errorMessage = "Input file does not exist: " + inputPath;
        UpdateStatistics(result);
        return result;
    }
    
    result.originalSize = GetFileSize(inputPath);
    
    // Route to appropriate cooker based on asset type
    if (assetType == "mesh") {
        result = CookMesh(inputPath);
    } else if (assetType == "texture") {
        result = CookTexture(inputPath);
    } else if (assetType == "material") {
        result = CookMaterial(inputPath);
    } else if (assetType == "animation") {
        result = CookAnimation(inputPath);
    } else {
        result.errorMessage = "Unknown asset type: " + assetType;
        UpdateStatistics(result);
        return result;
    }
    
    // Register in asset registry if available
    if (m_registry && result.success && result.assetGUID.IsValid()) {
        if (!m_registry->IsRegistered(result.assetGUID)) {
            m_registry->RegisterAsset(result.assetGUID, result.outputPath, assetType);
        }
    }
    
    UpdateStatistics(result);
    return result;
}

std::vector<CookingResult> AssetCooker::CookAssets(const std::vector<std::string>& inputPaths, const std::string& assetType) {
    std::vector<CookingResult> results;
    results.reserve(inputPaths.size());
    
    for (const auto& path : inputPaths) {
        results.push_back(CookAsset(path, assetType));
    }
    
    return results;
}

CookingResult AssetCooker::CookMesh(const std::string& inputPath) {
    CookingResult result;
    result.inputPath = inputPath;
    result.assetType = "mesh";
    result.originalSize = GetFileSize(inputPath);
    
    // Generate GUID for cooked mesh
    if (m_registry) {
        result.assetGUID = m_registry->GetGUID(inputPath);
        if (!result.assetGUID.IsValid()) {
            result.assetGUID = m_registry->RegisterAsset(inputPath, "mesh");
        }
    } else {
        result.assetGUID = AssetGUID::Generate();
    }
    
    // Output path: .omesh (OpenEye Mesh)
    std::string outputPath = GetOutputPath(inputPath, ".omesh");
    result.outputPath = outputPath;
    
    try {
        // For now, we'll create a basic cooked format
        // In a full implementation, this would:
        // 1. Load mesh via MeshLoader
        // 2. Optimize indices/vertices
        // 3. Compress geometry (Draco or meshopt)
        // 4. Serialize to binary format
        
        // Create a simple JSON representation for now
        // (In production, this would be a binary format)
        nlohmann::json meshJson;
        meshJson["version"] = 1;
        meshJson["sourcePath"] = inputPath;
        meshJson["compressed"] = m_options.compressMesh;
        meshJson["optimized"] = m_options.optimizeMesh;
        meshJson["optimizationLevel"] = m_options.meshOptimizationLevel;
        
        // Write to file
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            result.errorMessage = "Failed to open output file: " + outputPath;
            return result;
        }
        
        file << meshJson.dump(2);
        file.close();
        
        result.cookedSize = GetFileSize(outputPath);
        result.compressionRatio = result.originalSize > 0
            ? 1.0f - (static_cast<float>(result.cookedSize) / static_cast<float>(result.originalSize))
            : 0.0f;
        result.success = true;
        
        if (m_options.verbose) {
            OE_INFO_F("Cooked mesh: %s -> %s (%.2f%% compression)", 
                     inputPath.c_str(), outputPath.c_str(), result.compressionRatio * 100.0f);
        }
    } catch (const std::exception& e) {
        result.errorMessage = "Exception during mesh cooking: " + std::string(e.what());
    }
    
    return result;
}

CookingResult AssetCooker::CookTexture(const std::string& inputPath) {
    CookingResult result;
    result.inputPath = inputPath;
    result.assetType = "texture";
    result.originalSize = GetFileSize(inputPath);
    
    // Generate GUID
    if (m_registry) {
        result.assetGUID = m_registry->GetGUID(inputPath);
        if (!result.assetGUID.IsValid()) {
            result.assetGUID = m_registry->RegisterAsset(inputPath, "texture");
        }
    } else {
        result.assetGUID = AssetGUID::Generate();
    }
    
    // Output path: .otex (OpenEye Texture)
    std::string outputPath = GetOutputPath(inputPath, ".otex");
    result.outputPath = outputPath;
    
    try {
        // Create texture metadata
        nlohmann::json textureJson;
        textureJson["version"] = 1;
        textureJson["sourcePath"] = inputPath;
        textureJson["compressed"] = m_options.compressTexture;
        textureJson["format"] = m_options.textureFormat;
        textureJson["generateMipmaps"] = m_options.generateMipmaps;
        textureJson["maxSize"] = m_options.maxTextureSize;
        
        // Write to file
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            result.errorMessage = "Failed to open output file: " + outputPath;
            return result;
        }
        
        file << textureJson.dump(2);
        file.close();
        
        result.cookedSize = GetFileSize(outputPath);
        result.compressionRatio = result.originalSize > 0
            ? 1.0f - (static_cast<float>(result.cookedSize) / static_cast<float>(result.originalSize))
            : 0.0f;
        result.success = true;
        
        if (m_options.verbose) {
            OE_INFO_F("Cooked texture: %s -> %s (%.2f%% compression)", 
                     inputPath.c_str(), outputPath.c_str(), result.compressionRatio * 100.0f);
        }
    } catch (const std::exception& e) {
        result.errorMessage = "Exception during texture cooking: " + std::string(e.what());
    }
    
    return result;
}

CookingResult AssetCooker::CookMaterial(const std::string& inputPath) {
    CookingResult result;
    result.inputPath = inputPath;
    result.assetType = "material";
    result.originalSize = GetFileSize(inputPath);
    
    // Generate GUID
    if (m_registry) {
        result.assetGUID = m_registry->GetGUID(inputPath);
        if (!result.assetGUID.IsValid()) {
            result.assetGUID = m_registry->RegisterAsset(inputPath, "material");
        }
    } else {
        result.assetGUID = AssetGUID::Generate();
    }
    
    // Output path: .omat (OpenEye Material)
    std::string outputPath = GetOutputPath(inputPath, ".omat");
    result.outputPath = outputPath;
    
    try {
        // Load material (if it exists as a file, otherwise create from defaults)
        nlohmann::json materialJson;
        
        if (std::filesystem::exists(inputPath)) {
            std::ifstream inputFile(inputPath);
            if (inputFile.is_open()) {
                inputFile >> materialJson;
                inputFile.close();
            }
        }
        
        // Ensure version and metadata
        materialJson["version"] = 1;
        materialJson["sourcePath"] = inputPath;
        materialJson["serialized"] = m_options.serializeMaterial;
        materialJson["embedTextures"] = m_options.embedTextures;
        
        // Write to file
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            result.errorMessage = "Failed to open output file: " + outputPath;
            return result;
        }
        
        file << materialJson.dump(2);
        file.close();
        
        result.cookedSize = GetFileSize(outputPath);
        result.compressionRatio = result.originalSize > 0
            ? 1.0f - (static_cast<float>(result.cookedSize) / static_cast<float>(result.originalSize))
            : 0.0f;
        result.success = true;
        
        if (m_options.verbose) {
            OE_INFO_F("Cooked material: %s -> %s", inputPath.c_str(), outputPath.c_str());
        }
    } catch (const std::exception& e) {
        result.errorMessage = "Exception during material cooking: " + std::string(e.what());
    }
    
    return result;
}

CookingResult AssetCooker::CookAnimation(const std::string& inputPath) {
    CookingResult result;
    result.inputPath = inputPath;
    result.assetType = "animation";
    result.originalSize = GetFileSize(inputPath);
    
    // Generate GUID
    if (m_registry) {
        result.assetGUID = m_registry->GetGUID(inputPath);
        if (!result.assetGUID.IsValid()) {
            result.assetGUID = m_registry->RegisterAsset(inputPath, "animation");
        }
    } else {
        result.assetGUID = AssetGUID::Generate();
    }
    
    // Output path: .oanim (OpenEye Animation)
    std::string outputPath = GetOutputPath(inputPath, ".oanim");
    result.outputPath = outputPath;
    
    try {
        // Create animation metadata
        nlohmann::json animJson;
        animJson["version"] = 1;
        animJson["sourcePath"] = inputPath;
        animJson["compressed"] = m_options.compressAnimation;
        animJson["tolerance"] = m_options.animationTolerance;
        animJson["removeRedundantKeys"] = m_options.removeRedundantKeys;
        
        // Write to file
        std::ofstream file(outputPath);
        if (!file.is_open()) {
            result.errorMessage = "Failed to open output file: " + outputPath;
            return result;
        }
        
        file << animJson.dump(2);
        file.close();
        
        result.cookedSize = GetFileSize(outputPath);
        result.compressionRatio = result.originalSize > 0
            ? 1.0f - (static_cast<float>(result.cookedSize) / static_cast<float>(result.originalSize))
            : 0.0f;
        result.success = true;
        
        if (m_options.verbose) {
            OE_INFO_F("Cooked animation: %s -> %s (%.2f%% compression)", 
                     inputPath.c_str(), outputPath.c_str(), result.compressionRatio * 100.0f);
        }
    } catch (const std::exception& e) {
        result.errorMessage = "Exception during animation cooking: " + std::string(e.what());
    }
    
    return result;
}

void AssetCooker::ResetStatistics() {
    m_statistics = Statistics();
}

} // namespace Assets
} // namespace OpenEye

