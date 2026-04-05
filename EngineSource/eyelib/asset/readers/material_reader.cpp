#include "asset/readers/material_reader.h"
#include "asset/core/material_format.h"

#include <fstream>
#include <vector>
#include <array>
#include <cstring>

namespace asset {

namespace {

static bool ReadU32LE(const uint8_t* data, size_t size, size_t& pos, uint32_t& v) {
    if (pos + 4u > size) return false;
    v = static_cast<uint32_t>(data[pos]) | (static_cast<uint32_t>(data[pos + 1]) << 8u) |
        (static_cast<uint32_t>(data[pos + 2]) << 16u) | (static_cast<uint32_t>(data[pos + 3]) << 24u);
    pos += 4u;
    return true;
}

static void AppendU32LE(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xffu));
    buf.push_back(static_cast<uint8_t>((v >> 8u) & 0xffu));
    buf.push_back(static_cast<uint8_t>((v >> 16u) & 0xffu));
    buf.push_back(static_cast<uint8_t>((v >> 24u) & 0xffu));
}

static void AppendF32LE(std::vector<uint8_t>& buf, float f) {
    uint32_t u = 0;
    std::memcpy(&u, &f, sizeof(float));
    AppendU32LE(buf, u);
}

static void AppendBytes(std::vector<uint8_t>& buf, const std::string& s) {
    buf.insert(buf.end(), s.begin(), s.end());
}

/** Optional little-endian trailer after MAT1 v3 string blob: MATX + shader path + keyword/float/vec3/vec4 tables. */
static bool ParseMatXTrailer(const uint8_t* data, size_t size, size_t& pos, LoadedMaterial& out) {
    out.shaderAssetPath.clear();
    out.keywordPairs.clear();
    out.floatPairs.clear();
    out.vec3Pairs.clear();
    out.vec4Pairs.clear();
    if (pos + 4u > size) return true;
    if (std::memcmp(data + pos, "MATX", 4) != 0) return true;
    pos += 4u;

    uint32_t splen = 0;
    if (!ReadU32LE(data, size, pos, splen)) return false;
    if (splen > size - pos) return false;
    out.shaderAssetPath.assign(reinterpret_cast<const char*>(data + pos), splen);
    pos += splen;

    uint32_t kc = 0;
    if (!ReadU32LE(data, size, pos, kc)) return false;
    for (uint32_t i = 0; i < kc; ++i) {
        uint32_t kl = 0;
        if (!ReadU32LE(data, size, pos, kl)) return false;
        if (kl > size - pos) return false;
        std::string k(reinterpret_cast<const char*>(data + pos), kl);
        pos += kl;
        if (pos >= size) return false;
        const uint8_t on = data[pos++];
        out.keywordPairs.push_back({std::move(k), on != 0});
    }

    uint32_t fc = 0;
    if (!ReadU32LE(data, size, pos, fc)) return false;
    for (uint32_t i = 0; i < fc; ++i) {
        uint32_t kl = 0;
        if (!ReadU32LE(data, size, pos, kl)) return false;
        if (kl > size - pos) return false;
        std::string k(reinterpret_cast<const char*>(data + pos), kl);
        pos += kl;
        uint32_t uv = 0;
        if (!ReadU32LE(data, size, pos, uv)) return false;
        float fv = 0.f;
        std::memcpy(&fv, &uv, sizeof(float));
        out.floatPairs.push_back({std::move(k), fv});
    }

    uint32_t v3c = 0;
    if (pos + 4u <= size) {
        if (!ReadU32LE(data, size, pos, v3c)) return false;
        for (uint32_t i = 0; i < v3c; ++i) {
            uint32_t kl = 0;
            if (!ReadU32LE(data, size, pos, kl)) return false;
            if (kl > size - pos) return false;
            std::string k(reinterpret_cast<const char*>(data + pos), kl);
            pos += kl;
            std::array<float, 3> v{};
            for (int j = 0; j < 3; ++j) {
                uint32_t uv = 0;
                if (!ReadU32LE(data, size, pos, uv)) return false;
                std::memcpy(&v[static_cast<size_t>(j)], &uv, sizeof(float));
            }
            out.vec3Pairs.push_back({std::move(k), v});
        }
    }

    uint32_t v4c = 0;
    if (pos + 4u <= size) {
        if (!ReadU32LE(data, size, pos, v4c)) return false;
        for (uint32_t i = 0; i < v4c; ++i) {
            uint32_t kl = 0;
            if (!ReadU32LE(data, size, pos, kl)) return false;
            if (kl > size - pos) return false;
            std::string k(reinterpret_cast<const char*>(data + pos), kl);
            pos += kl;
            std::array<float, 4> v{};
            for (int j = 0; j < 4; ++j) {
                uint32_t uv = 0;
                if (!ReadU32LE(data, size, pos, uv)) return false;
                std::memcpy(&v[static_cast<size_t>(j)], &uv, sizeof(float));
            }
            out.vec4Pairs.push_back({std::move(k), v});
        }
    }

    return true;
}

static void AppendMatXTrailer(std::vector<uint8_t>& buf, const LoadedMaterial& in) {
    const bool need = !in.shaderAssetPath.empty() || !in.keywordPairs.empty() || !in.floatPairs.empty() ||
                      !in.vec3Pairs.empty() || !in.vec4Pairs.empty();
    if (!need) return;

    buf.push_back('M');
    buf.push_back('A');
    buf.push_back('T');
    buf.push_back('X');
    AppendU32LE(buf, static_cast<uint32_t>(in.shaderAssetPath.size()));
    AppendBytes(buf, in.shaderAssetPath);

    AppendU32LE(buf, static_cast<uint32_t>(in.keywordPairs.size()));
    for (const auto& kv : in.keywordPairs) {
        AppendU32LE(buf, static_cast<uint32_t>(kv.first.size()));
        AppendBytes(buf, kv.first);
        buf.push_back(kv.second ? static_cast<uint8_t>(1) : static_cast<uint8_t>(0));
    }

    AppendU32LE(buf, static_cast<uint32_t>(in.floatPairs.size()));
    for (const auto& kv : in.floatPairs) {
        AppendU32LE(buf, static_cast<uint32_t>(kv.first.size()));
        AppendBytes(buf, kv.first);
        AppendF32LE(buf, kv.second);
    }

    AppendU32LE(buf, static_cast<uint32_t>(in.vec3Pairs.size()));
    for (const auto& kv : in.vec3Pairs) {
        AppendU32LE(buf, static_cast<uint32_t>(kv.first.size()));
        AppendBytes(buf, kv.first);
        for (int j = 0; j < 3; ++j) AppendF32LE(buf, kv.second[static_cast<size_t>(j)]);
    }

    AppendU32LE(buf, static_cast<uint32_t>(in.vec4Pairs.size()));
    for (const auto& kv : in.vec4Pairs) {
        AppendU32LE(buf, static_cast<uint32_t>(kv.first.size()));
        AppendBytes(buf, kv.first);
        for (int j = 0; j < 4; ++j) AppendF32LE(buf, kv.second[static_cast<size_t>(j)]);
    }
}

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
            if (len == 0) {
                s.clear();
                return true;
            }
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
        out.albedoColorR = 1.f;
        out.albedoColorG = 1.f;
        out.albedoColorB = 1.f;
        return true;
    }

    if (version == kVersionV3) {
        if (size < sizeof(MaterialHeaderV3)) return false;
        MaterialHeaderV3 h{};
        std::memcpy(&h, data, sizeof(MaterialHeaderV3));
        size_t pos = sizeof(MaterialHeaderV3);
        auto readStr = [&](uint32_t len, std::string& s) -> bool {
            if (len == 0) {
                s.clear();
                return true;
            }
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
        out.emissionR = h.emissionR;
        out.emissionG = h.emissionG;
        out.emissionB = h.emissionB;
        out.aoValue = h.aoValue;
        out.alpha = h.alpha;
        if (!readStr(h.aoPathLen, out.aoPath)) return false;
        if (!readStr(h.emissivePathLen, out.emissivePath)) return false;
        if (!readStr(h.mrPathLen, out.metallicRoughnessPath)) return false;
        if (!readStr(h.fragmentSnippetLen, out.fragmentSnippet)) return false;
        if (!readStr(h.shadingSnippetLen, out.shadingSnippet)) return false;
        out.shadingModel = h.shadingModel;
        out.blending = h.blending;
        out.flags = h.flags;
        out.reflectiveStrength = h.reflectiveStrength;
        out.materialShaderId = h.materialShaderId;
        if (!ParseMatXTrailer(data, size, pos, out)) return false;
        return true;
    }

    if (version != kVersion || size < sizeof(MaterialHeader)) return false;
    MaterialHeader h{};
    std::memcpy(&h, data, sizeof(MaterialHeader));
    size_t pos = sizeof(MaterialHeader);
    auto readStr = [&](uint32_t len, std::string& s) -> bool {
        if (len == 0) {
            s.clear();
            return true;
        }
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

}  // namespace

bool MaterialReader::Read(const std::string& path, LoadedMaterial& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const std::streamsize sz = file.tellg();
    if (sz < 0) return false;
    file.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    if (!file.read(reinterpret_cast<char*>(buf.data()), sz)) return false;
    return ParseMaterialBytes(buf.data(), buf.size(), out);
}

bool MaterialReader::Read(const std::vector<uint8_t>& bytes, LoadedMaterial& out) {
    return ParseMaterialBytes(bytes.data(), bytes.size(), out);
}

bool MaterialReader::Write(const std::string& path, const LoadedMaterial& in) {
    using namespace MaterialFormat;
    MaterialHeaderV3 h{};
    h.nameLen = static_cast<uint32_t>(in.name.size());
    h.albedoPathLen = static_cast<uint32_t>(in.albedoPath.size());
    h.normalPathLen = static_cast<uint32_t>(in.normalPath.size());
    h.metallicPathLen = static_cast<uint32_t>(in.metallicPath.size());
    h.roughnessPathLen = static_cast<uint32_t>(in.roughnessPath.size());
    h.metallicValue = in.metallicValue;
    h.roughnessValue = in.roughnessValue;
    h.albedoColorR = in.albedoColorR;
    h.albedoColorG = in.albedoColorG;
    h.albedoColorB = in.albedoColorB;
    h.emissionR = in.emissionR;
    h.emissionG = in.emissionG;
    h.emissionB = in.emissionB;
    h.aoValue = in.aoValue;
    h.alpha = in.alpha;
    h.aoPathLen = static_cast<uint32_t>(in.aoPath.size());
    h.emissivePathLen = static_cast<uint32_t>(in.emissivePath.size());
    h.mrPathLen = static_cast<uint32_t>(in.metallicRoughnessPath.size());
    h.fragmentSnippetLen = static_cast<uint32_t>(in.fragmentSnippet.size());
    h.shadingSnippetLen = static_cast<uint32_t>(in.shadingSnippet.size());
    h.shadingModel = in.shadingModel;
    h.blending = in.blending;
    h.flags = in.flags;
    h.reflectiveStrength = in.reflectiveStrength;
    h.materialShaderId = in.materialShaderId;

    std::vector<uint8_t> buf;
    buf.resize(sizeof(MaterialHeaderV3));
    std::memcpy(buf.data(), &h, sizeof(MaterialHeaderV3));
    AppendBytes(buf, in.name);
    AppendBytes(buf, in.albedoPath);
    AppendBytes(buf, in.normalPath);
    AppendBytes(buf, in.metallicPath);
    AppendBytes(buf, in.roughnessPath);
    AppendBytes(buf, in.aoPath);
    AppendBytes(buf, in.emissivePath);
    AppendBytes(buf, in.metallicRoughnessPath);
    AppendBytes(buf, in.fragmentSnippet);
    AppendBytes(buf, in.shadingSnippet);
    AppendMatXTrailer(buf, in);

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    return static_cast<bool>(f);
}

}  // namespace asset
