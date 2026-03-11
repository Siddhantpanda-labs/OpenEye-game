#include "asset/writers/material_writer.h"
#include "asset/core/material_format.h"

#include <fstream>
#include <cstring>

namespace asset {

bool MaterialWriter::Write(const ImportMaterial& material, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;

    using namespace MaterialFormat;

    MaterialHeader header;
    header.nameLen = static_cast<uint32_t>(material.name.size());
    header.albedoPathLen = static_cast<uint32_t>(material.albedo.path.size());
    header.normalPathLen = static_cast<uint32_t>(material.normal.path.size());
    header.metallicPathLen = static_cast<uint32_t>(material.metallic.path.size());
    header.roughnessPathLen = static_cast<uint32_t>(material.roughness.path.size());
    header.metallicValue = material.metallicValue;
    header.roughnessValue = material.roughnessValue;
    header.albedoColorR = material.albedoColorR;
    header.albedoColorG = material.albedoColorG;
    header.albedoColorB = material.albedoColorB;

    if (!file.write(reinterpret_cast<const char*>(&header), sizeof(header))) return false;
    if (!material.name.empty() && !file.write(material.name.data(), material.name.size())) return false;
    if (!material.albedo.path.empty() && !file.write(material.albedo.path.data(), material.albedo.path.size())) return false;
    if (!material.normal.path.empty() && !file.write(material.normal.path.data(), material.normal.path.size())) return false;
    if (!material.metallic.path.empty() && !file.write(material.metallic.path.data(), material.metallic.path.size())) return false;
    if (!material.roughness.path.empty() && !file.write(material.roughness.path.data(), material.roughness.path.size())) return false;

    return true;
}

} // namespace asset
