#pragma once

#include <string>

namespace asset_pipe {

struct LoadedMaterial {
    std::string name;
    std::string albedoPath;
    std::string normalPath;
    std::string metallicPath;
    std::string roughnessPath;
    float metallicValue = 0.f;
    float roughnessValue = 0.5f;
    float albedoColorR = 1.f;
    float albedoColorG = 1.f;
    float albedoColorB = 1.f;
};

/** Reads canonical .material binary into LoadedMaterial. Returns true on success. */
class MaterialReader {
public:
    static bool Read(const std::string& path, LoadedMaterial& out);
};

} // namespace asset_pipe
