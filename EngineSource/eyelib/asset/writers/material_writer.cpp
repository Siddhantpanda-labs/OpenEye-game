#include "asset/writers/material_writer.h"
#include "asset/readers/material_reader.h"

#include <fstream>
#include <vector>

namespace asset {

bool MaterialWriter::Write(const ImportMaterial& material, const std::string& path) {
    LoadedMaterial lm;
    lm.name = material.name;
    lm.albedoPath = material.albedo.textureAssetId;
    lm.normalPath = material.normal.textureAssetId;
    lm.metallicPath = material.metallic.textureAssetId;
    lm.roughnessPath = material.roughness.textureAssetId;
    lm.metallicValue = material.metallicValue;
    lm.roughnessValue = material.roughnessValue;
    lm.albedoColorR = material.albedoColorR;
    lm.albedoColorG = material.albedoColorG;
    lm.albedoColorB = material.albedoColorB;

    std::vector<uint8_t> buf;
    if (!MaterialReader::EncodeToBuffer(lm, buf)) return false;

    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    if (!buf.empty() && !file.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size())))
        return false;
    return static_cast<bool>(file);
}

} // namespace asset
