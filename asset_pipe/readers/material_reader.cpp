#include "asset_pipe/readers/material_reader.h"
#include "asset_pipe/core/material_format.h"

#include <fstream>

namespace asset_pipe {

bool MaterialReader::Read(const std::string& path, LoadedMaterial& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    using namespace MaterialFormat;

    MaterialHeader header;
    if (!file.read(reinterpret_cast<char*>(&header), sizeof(header))) return false;
    if (header.magic != kMagic || header.version != kVersion) return false;

    out = LoadedMaterial{};
    if (header.nameLen > 0) {
        out.name.resize(header.nameLen);
        if (!file.read(&out.name[0], header.nameLen)) return false;
    }
    if (header.albedoPathLen > 0) {
        out.albedoPath.resize(header.albedoPathLen);
        if (!file.read(&out.albedoPath[0], header.albedoPathLen)) return false;
    }
    if (header.normalPathLen > 0) {
        out.normalPath.resize(header.normalPathLen);
        if (!file.read(&out.normalPath[0], header.normalPathLen)) return false;
    }
    if (header.metallicPathLen > 0) {
        out.metallicPath.resize(header.metallicPathLen);
        if (!file.read(&out.metallicPath[0], header.metallicPathLen)) return false;
    }
    if (header.roughnessPathLen > 0) {
        out.roughnessPath.resize(header.roughnessPathLen);
        if (!file.read(&out.roughnessPath[0], header.roughnessPathLen)) return false;
    }
    out.metallicValue = header.metallicValue;
    out.roughnessValue = header.roughnessValue;
    return true;
}

} // namespace asset_pipe
