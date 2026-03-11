#include "asset/readers/material_reader.h"
#include "asset/core/material_format.h"

#include <fstream>
#include <cstring>

namespace asset {

namespace {

bool ParseMaterialBytes(const uint8_t* data, size_t size, LoadedMaterial& out) {
    if (!data || size < sizeof(uint32_t) * 2u) return false;

    using namespace MaterialFormat;
    const uint32_t magic = *reinterpret_cast<const uint32_t*>(data + 0);
    const uint32_t version = *reinterpret_cast<const uint32_t*>(data + 4);
    if (magic != kMagic) return false;

    out = LoadedMaterial{};
    if (version == kVersionV1) {
        struct V1Header {
            uint32_t magic, version, nameLen, albedoPathLen, normalPathLen, metallicPathLen, roughnessPathLen;
            float metallicValue, roughnessValue;
        };
        if (size < sizeof(V1Header)) return false;
        V1Header h{};
        std::memcpy(&h, data, sizeof(V1Header));
        size_t pos = sizeof(V1Header);
        auto readStr = [&](uint32_t len, std::string& s) -> bool {
            if (len == 0) { s.clear(); return true; }
            if (pos > size || static_cast<size_t>(len) > (size - pos)) return false;
            s.assign(reinterpret_cast<const char*>(data + pos), len);
            pos += len;
            return true;
        };
        if (!readStr(h.nameLen, out.name)) return false;
        if (!readStr(h.albedoPathLen, out.albedoPath)) return false;
        if (!readStr(h.normalPathLen, out.normalPath)) return false;
        if (!readStr(h.metallicPathLen, out.metallicPath)) return false;
        if (!readStr(h.roughnessPathLen, out.roughnessPath)) return false;
        out.metallicValue = h.metallicValue;
        out.roughnessValue = h.roughnessValue;
        out.albedoColorR = 1.f; out.albedoColorG = 1.f; out.albedoColorB = 1.f;
        return true;
    }

    if (version != kVersion || size < sizeof(MaterialHeader)) return false;
    MaterialHeader h{};
    std::memcpy(&h, data, sizeof(MaterialHeader));
    size_t pos = sizeof(MaterialHeader);
    auto readStr = [&](uint32_t len, std::string& s) -> bool {
        if (len == 0) { s.clear(); return true; }
        if (pos > size || static_cast<size_t>(len) > (size - pos)) return false;
        s.assign(reinterpret_cast<const char*>(data + pos), len);
        pos += len;
        return true;
    };
    if (!readStr(h.nameLen, out.name)) return false;
    if (!readStr(h.albedoPathLen, out.albedoPath)) return false;
    if (!readStr(h.normalPathLen, out.normalPath)) return false;
    if (!readStr(h.metallicPathLen, out.metallicPath)) return false;
    if (!readStr(h.roughnessPathLen, out.roughnessPath)) return false;
    out.metallicValue = h.metallicValue;
    out.roughnessValue = h.roughnessValue;
    out.albedoColorR = h.albedoColorR;
    out.albedoColorG = h.albedoColorG;
    out.albedoColorB = h.albedoColorB;
    return true;
}

} // namespace

bool MaterialReader::Read(const std::string& path, LoadedMaterial& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    using namespace MaterialFormat;

    MaterialHeader header;
    if (!file.read(reinterpret_cast<char*>(&header), sizeof(header))) {
        // Try reading as v1 (smaller header without albedo color fields).
        file.clear();
        file.seekg(0);
        struct V1Header {
            uint32_t magic, version, nameLen, albedoPathLen, normalPathLen, metallicPathLen, roughnessPathLen;
            float metallicValue, roughnessValue;
        } v1;
        if (!file.read(reinterpret_cast<char*>(&v1), sizeof(v1))) return false;
        if (v1.magic != kMagic || v1.version != kVersionV1) return false;
        header.magic = v1.magic; header.version = kVersion;
        header.nameLen = v1.nameLen; header.albedoPathLen = v1.albedoPathLen;
        header.normalPathLen = v1.normalPathLen; header.metallicPathLen = v1.metallicPathLen;
        header.roughnessPathLen = v1.roughnessPathLen;
        header.metallicValue = v1.metallicValue; header.roughnessValue = v1.roughnessValue;
        header.albedoColorR = 1.f; header.albedoColorG = 1.f; header.albedoColorB = 1.f;
    } else if (header.magic != kMagic) {
        return false;
    } else if (header.version == kVersionV1) {
        // V2 header was read but file is actually v1 (shorter); re-read as v1.
        file.clear();
        file.seekg(0);
        struct V1Header {
            uint32_t magic, version, nameLen, albedoPathLen, normalPathLen, metallicPathLen, roughnessPathLen;
            float metallicValue, roughnessValue;
        } v1;
        if (!file.read(reinterpret_cast<char*>(&v1), sizeof(v1))) return false;
        header.nameLen = v1.nameLen; header.albedoPathLen = v1.albedoPathLen;
        header.normalPathLen = v1.normalPathLen; header.metallicPathLen = v1.metallicPathLen;
        header.roughnessPathLen = v1.roughnessPathLen;
        header.metallicValue = v1.metallicValue; header.roughnessValue = v1.roughnessValue;
        header.albedoColorR = 1.f; header.albedoColorG = 1.f; header.albedoColorB = 1.f;
    } else if (header.version != kVersion) {
        return false;
    }

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
    out.albedoColorR = header.albedoColorR;
    out.albedoColorG = header.albedoColorG;
    out.albedoColorB = header.albedoColorB;
    return true;
}

bool MaterialReader::Read(const std::vector<uint8_t>& bytes, LoadedMaterial& out) {
    return ParseMaterialBytes(bytes.data(), bytes.size(), out);
}

} // namespace asset
