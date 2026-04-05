#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace asset {

struct LoadedMaterial {
    std::string name;
    std::string albedoPath;
    std::string normalPath;
    std::string metallicPath;
    std::string roughnessPath;
    std::string aoPath;
    std::string emissivePath;
    std::string metallicRoughnessPath;
    float metallicValue = 0.f;
    float roughnessValue = 0.5f;
    float albedoColorR = 1.f;
    float albedoColorG = 1.f;
    float albedoColorB = 1.f;
    float emissionR = 0.f;
    float emissionG = 0.f;
    float emissionB = 0.f;
    float aoValue = 1.f;
    float alpha = 1.f;
    uint8_t shadingModel = 0;
    uint8_t blending = 0;
    uint8_t flags = 0;  // bit0 two-sided, bit1 refraction
    float reflectiveStrength = 0.f;
    uint64_t materialShaderId = 0u;
    std::string fragmentSnippet;
    std::string shadingSnippet;
    /** Optional MAT1 v3 trailer ("MATX"); persisted by MaterialReader::Write when non-empty. */
    std::string shaderAssetPath;
    std::vector<std::pair<std::string, bool>> keywordPairs;
    std::vector<std::pair<std::string, float>> floatPairs;
    std::vector<std::pair<std::string, std::array<float, 3>>> vec3Pairs;
    std::vector<std::pair<std::string, std::array<float, 4>>> vec4Pairs;
};

/** Reads canonical .mat binary into LoadedMaterial. Returns true on success. */
class MaterialReader {
public:
    static bool Read(const std::string& path, LoadedMaterial& out);
    static bool Read(const std::vector<uint8_t>& bytes, LoadedMaterial& out);
    /** Writes MAT1 v3. Overwrites path when possible. */
    static bool Write(const std::string& path, const LoadedMaterial& in);
};

} // namespace asset
