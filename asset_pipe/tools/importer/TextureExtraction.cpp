#include "asset_pipe/tools/importer/TextureExtraction.h"
#include "asset_pipe/core/import_scene.h"
#include "asset_pipe/core/import_material.h"
#include "asset_pipe/core/import_texture.h"

#include <assimp/material.h>
#include <assimp/scene.h>
#include <assimp/texture.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace asset_pipe {

namespace {

std::string NormalizePath(const std::string& path) {
    std::string p = path;
    while (!p.empty() && (p.back() == ' ' || p.back() == '\t')) p.pop_back();
    return p;
}

bool ReadFileBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.seekg(0, std::ios::end);
    const std::streamoff n = file.tellg();
    if (n <= 0) return false;
    file.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(n));
    return file.read(reinterpret_cast<char*>(out.data()), n).good();
}

bool ProbeInfoFromMemory(const uint8_t* data, size_t size, uint32_t& w, uint32_t& h, uint32_t& ch) {
    int iw = 0, ih = 0, ich = 0;
    if (!stbi_info_from_memory(reinterpret_cast<const unsigned char*>(data), static_cast<int>(size), &iw, &ih, &ich))
        return false;
    if (iw <= 0 || ih <= 0) return false;
    w = static_cast<uint32_t>(iw);
    h = static_cast<uint32_t>(ih);
    ch = static_cast<uint32_t>(ich > 0 ? ich : 4);
    return true;
}

/** Decode embedded Assimp texture or preserve compressed bytes when possible. */
bool DecodeEmbeddedTexture(const aiTexture* tex, ImportTexture& out) {
    if (!tex || !tex->pcData) return false;
    out = ImportTexture{};
    out.name.clear();
    out.sourcePath.clear();

    if (tex->mHeight != 0) {
        // Uncompressed: aiTexel is RGBA
        const unsigned w = tex->mWidth;
        const unsigned h = tex->mHeight;
        const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h);
        out.width = w;
        out.height = h;
        out.channels = 4;
        out.pixels.resize(count * 4);
        const aiTexel* src = tex->pcData;
        for (size_t i = 0; i < count; ++i) {
            out.pixels[i * 4 + 0] = src[i].r;
            out.pixels[i * 4 + 1] = src[i].g;
            out.pixels[i * 4 + 2] = src[i].b;
            out.pixels[i * 4 + 3] = src[i].a;
        }
        return true;
    }

    // Compressed: preserve original bytes for better package compression.
    const uint8_t* src = reinterpret_cast<const uint8_t*>(tex->pcData);
    const size_t byteSize = static_cast<size_t>(tex->mWidth);
    if (byteSize == 0) return false;
    out.encodedData.assign(src, src + byteSize);
    if (!ProbeInfoFromMemory(src, byteSize, out.width, out.height, out.channels)) {
        // Fallback decode when header probing fails.
        int w = 0, h = 0, ch = 0;
        stbi_set_flip_vertically_on_load(0);
        unsigned char* decoded = stbi_load_from_memory(
            reinterpret_cast<const unsigned char*>(src),
            static_cast<int>(byteSize), &w, &h, &ch, 4);
        if (!decoded) return false;
        out.width = static_cast<uint32_t>(w);
        out.height = static_cast<uint32_t>(h);
        out.channels = 4;
        const size_t total = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
        out.pixels.assign(decoded, decoded + total);
        out.encodedData.clear();
        stbi_image_free(decoded);
    }
    return true;
}

/** Load image file preserving encoded bytes; fallback to RGBA decode. */
bool LoadFileTexture(const std::string& path, ImportTexture& out) {
    out = ImportTexture{};
    out.sourcePath = path;

    std::vector<uint8_t> bytes;
    if (ReadFileBytes(path, bytes)) {
        uint32_t w = 0, h = 0, ch = 0;
        if (ProbeInfoFromMemory(bytes.data(), bytes.size(), w, h, ch)) {
            out.width = w;
            out.height = h;
            out.channels = ch;
            out.encodedData = std::move(bytes);
            return true;
        }
    }

    int w = 0, h = 0, ch = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* decoded = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!decoded) return false;
    out.width = static_cast<uint32_t>(w);
    out.height = static_cast<uint32_t>(h);
    out.channels = 4;
    const size_t total = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
    out.pixels.assign(decoded, decoded + total);
    stbi_image_free(decoded);
    return true;
}

/** Resolve one texture path (embedded *N or file) and append to out.textures. Returns index or -1. */
int ResolveAndAddTexture(const std::string& rawPath, const aiScene* scene,
    const std::filesystem::path& sourceDir, const std::string& sourceName,
    ImportScene& out, std::unordered_map<std::string, int>& pathToIndex) {
    std::string key = NormalizePath(rawPath);
    if (key.empty()) return -1;
    auto it = pathToIndex.find(key);
    if (it != pathToIndex.end()) return it->second;

    ImportTexture imp;
    bool ok = false;

    if (rawPath.size() >= 2 && rawPath[0] == '*') {
        // Embedded: *0, *1, or *filename
        int index = -1;
        try { index = std::stoi(rawPath.substr(1)); } catch (...) {}
        const aiTexture* emb = nullptr;
        if (index >= 0 && scene && scene->mNumTextures > 0 &&
            static_cast<unsigned>(index) < scene->mNumTextures) {
            emb = scene->mTextures[static_cast<unsigned>(index)];
        }
        if (!emb && scene)
            emb = scene->GetEmbeddedTexture(rawPath.c_str());
        if (emb)
            ok = DecodeEmbeddedTexture(emb, imp);
    }

    if (!ok) {
        // File: try sourceDir / path, then path as-is
        std::filesystem::path tryPath = sourceDir / rawPath;
        if (std::filesystem::exists(tryPath))
            ok = LoadFileTexture(tryPath.string(), imp);
        if (!ok && std::filesystem::exists(rawPath))
            ok = LoadFileTexture(rawPath, imp);
        // FBX (and others) may reference embedded textures by path (e.g. "..\file.fbm\image.jpg") instead of *N
        if (!ok && scene) {
            const aiTexture* emb = scene->GetEmbeddedTexture(rawPath.c_str());
            if (emb)
                ok = DecodeEmbeddedTexture(emb, imp);
        }
    }

    if (!ok) return -1;

    const size_t idx = out.textures.size();
    imp.name = sourceName + "_texture_" + std::to_string(idx);
    out.textures.push_back(std::move(imp));
    pathToIndex[key] = static_cast<int>(idx);
    return static_cast<int>(idx);
}

void RemapMaterialPaths(ImportScene& out, std::unordered_map<std::string, int>& pathToIndex) {
    for (ImportMaterial& mat : out.materials) {
        auto setPath = [&](TextureRef& ref, const std::string& key) {
            if (ref.path.empty()) return;
            std::string k = NormalizePath(key);
            auto it = pathToIndex.find(k);
            if (it != pathToIndex.end() && it->second >= 0 &&
                static_cast<size_t>(it->second) < out.textures.size())
                ref.path = out.textures[static_cast<size_t>(it->second)].name + ".tex";
        };
        setPath(mat.albedo, mat.albedo.path);
        setPath(mat.normal, mat.normal.path);
        setPath(mat.metallic, mat.metallic.path);
        setPath(mat.roughness, mat.roughness.path);
    }
}

} // namespace

void ExtractTextures(const aiScene* scene, const char* sourcePath, const std::string& sourceName, ImportScene& out) {
    std::filesystem::path sourceDir = sourcePath ? std::filesystem::path(sourcePath).parent_path() : std::filesystem::path(".");
    std::unordered_map<std::string, int> pathToIndex;

    for (const ImportMaterial& mat : out.materials) {
        auto collect = [&](const std::string& path) {
            if (path.empty()) return;
            ResolveAndAddTexture(path, scene, sourceDir, sourceName, out, pathToIndex);
        };
        collect(mat.albedo.path);
        collect(mat.normal.path);
        collect(mat.metallic.path);
        collect(mat.roughness.path);
    }

    RemapMaterialPaths(out, pathToIndex);
}

} // namespace asset_pipe
