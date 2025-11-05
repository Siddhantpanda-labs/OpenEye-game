#pragma once

#include "AssetGUID.h"
#include "AssetRegistry.h"
#include <string>
#include <vector>
#include <memory>
#include <json.hpp>

namespace OpenEye {
namespace Assets {

/**
 * @brief Cooking options for different asset types.
 */
struct CookingOptions {
    // Mesh options
    bool compressMesh = true;
    bool optimizeMesh = true;
    float meshOptimizationLevel = 0.5f; // 0.0 = no optimization, 1.0 = maximum
    
    // Texture options
    bool compressTexture = true;
    std::string textureFormat = "BC7"; // BC1, BC3, BC7, ASTC_4x4, etc.
    bool generateMipmaps = true;
    int maxTextureSize = 4096;
    
    // Material options
    bool serializeMaterial = true;
    bool embedTextures = false; // Embed texture data in material JSON
    
    // Animation options
    bool compressAnimation = true;
    float animationTolerance = 0.001f; // Keyframe reduction tolerance
    bool removeRedundantKeys = true;
    
    // General options
    bool verbose = false;
    std::string outputDirectory = "assets/cooked/";
    
    // Serialization
    nlohmann::json ToJson() const;
    static CookingOptions FromJson(const nlohmann::json& json);
};

/**
 * @brief Cooking result for an asset.
 */
struct CookingResult {
    bool success = false;
    AssetGUID assetGUID;
    std::string inputPath;
    std::string outputPath;
    std::string errorMessage;
    size_t originalSize = 0;
    size_t cookedSize = 0;
    float compressionRatio = 0.0f; // 0.0 = no compression, 1.0 = 100% compression
    
    // Metadata
    std::string assetType;
    std::vector<AssetGUID> dependencies;
    
    // Serialization
    nlohmann::json ToJson() const;
};

/**
 * @brief Asset Cooker handles compression, optimization, and serialization of assets.
 * 
 * Features:
 * - Mesh compression and optimization
 * - Texture compression (BC/ASTC formats)
 * - Material JSON serialization
 * - Animation compression (keyframe reduction)
 * - Dependency tracking
 * - Batch cooking
 */
class AssetCooker {
public:
    AssetCooker();
    ~AssetCooker() = default;
    
    /**
     * @brief Set cooking options.
     */
    void SetOptions(const CookingOptions& options) { m_options = options; }
    const CookingOptions& GetOptions() const { return m_options; }
    
    /**
     * @brief Set asset registry (for dependency tracking).
     */
    void SetRegistry(AssetRegistry* registry) { m_registry = registry; }
    
    /**
     * @brief Cook a single asset.
     * @param inputPath Path to input asset file
     * @param assetType Type of asset ("mesh", "texture", "material", "animation")
     * @return Cooking result
     */
    CookingResult CookAsset(const std::string& inputPath, const std::string& assetType);
    
    /**
     * @brief Cook multiple assets.
     * @param inputPaths Vector of input file paths
     * @param assetType Type of assets
     * @return Vector of cooking results
     */
    std::vector<CookingResult> CookAssets(const std::vector<std::string>& inputPaths, const std::string& assetType);
    
    /**
     * @brief Cook a mesh asset.
     * @param inputPath Path to mesh file (FBX, OBJ, etc.)
     * @return Cooking result
     */
    CookingResult CookMesh(const std::string& inputPath);
    
    /**
     * @brief Cook a texture asset.
     * @param inputPath Path to texture file (PNG, JPG, TGA, etc.)
     * @return Cooking result
     */
    CookingResult CookTexture(const std::string& inputPath);
    
    /**
     * @brief Cook a material asset.
     * @param inputPath Path to material file
     * @return Cooking result
     */
    CookingResult CookMaterial(const std::string& inputPath);
    
    /**
     * @brief Cook an animation asset.
     * @param inputPath Path to animation file (FBX, etc.)
     * @return Cooking result
     */
    CookingResult CookAnimation(const std::string& inputPath);
    
    /**
     * @brief Get cooking statistics.
     */
    struct Statistics {
        size_t totalAssetsCooked = 0;
        size_t successfulAssets = 0;
        size_t failedAssets = 0;
        size_t totalOriginalSize = 0;
        size_t totalCookedSize = 0;
        float averageCompressionRatio = 0.0f;
    };
    
    const Statistics& GetStatistics() const { return m_statistics; }
    void ResetStatistics();

private:
    CookingOptions m_options;
    AssetRegistry* m_registry = nullptr;
    Statistics m_statistics;
    
    // Helper: Get output path for cooked asset
    std::string GetOutputPath(const std::string& inputPath, const std::string& extension) const;
    
    // Helper: Get file size
    size_t GetFileSize(const std::string& filepath) const;
    
    // Helper: Update statistics
    void UpdateStatistics(const CookingResult& result);
};

} // namespace Assets
} // namespace OpenEye

