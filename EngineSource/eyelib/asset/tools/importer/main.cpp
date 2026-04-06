/**
 * Asset pipeline importer.
 * Modes:
 *   default : model/<name>.omesh + textures/ + anims (materials embedded in omesh)
 *   --mat   : model/<name>.omesh + materials/*.mat + textures/ + anims
 *   --oea   : single <name>.oea only (no loose writes)
 */

#include "asset/tools/importer/SceneLoader.h"
#include "asset/tools/importer/SceneConverter.h"
#include <assimp/scene.h>
#include "asset/core/import_scene.h"
#include "asset/core/mesh_format.h"
#include "asset/readers/material_reader.h"
#include "asset/core/texture_format.h"
#include "asset/writers/material_writer.h"
#include "asset/writers/animation_writer.h"
#include "asset/writers/oea_writer.h"
#include "asset/core/oea_format.h"
#include "asset/meta/meta_file.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

static void Log(const char* msg) { std::cout << "[Importer] " << msg << std::endl; }

namespace {
using namespace asset;

enum class OutputMode {
    OmeshEmbeddedMaterials, // default
    OmeshSplitMaterials,    // --mat
    OeaOnly                 // --oea
};

template <typename T>
void AppendBytes(std::vector<uint8_t>& out, const T& v) {
    const auto* p = reinterpret_cast<const uint8_t*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}

void AppendRaw(std::vector<uint8_t>& out, const void* ptr, size_t size) {
    const auto* p = reinterpret_cast<const uint8_t*>(ptr);
    out.insert(out.end(), p, p + size);
}

size_t AlignUp(size_t value, size_t align) { return (value + align - 1) & ~(align - 1); }

bool SerializeMesh(const ImportMesh& mesh, std::vector<uint8_t>& out) {
    using namespace MeshFormat;
    out.clear();
    const uint32_t vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    const uint32_t indexCount = static_cast<uint32_t>(mesh.indices.size());
    const uint32_t submeshCount = static_cast<uint32_t>(mesh.submeshes.size());

    size_t offset = sizeof(MeshHeader);
    const uint32_t lodCount = 1u;
    size_t lodTableSize = AlignUp(lodCount * sizeof(MeshLOD), kAlignment);
    uint32_t lodOffset = static_cast<uint32_t>(offset); offset += lodTableSize;
    size_t submeshTableSize = AlignUp(submeshCount * sizeof(SubmeshRecord), kAlignment);
    uint32_t submeshOffset = static_cast<uint32_t>(offset); offset += submeshTableSize;
    size_t vertexDataSize = AlignUp(vertexCount * sizeof(VertexRecord), kAlignment);
    uint32_t vertexOffset = static_cast<uint32_t>(offset); offset += vertexDataSize;
    size_t indexDataSize = AlignUp(indexCount * sizeof(uint32_t), kAlignment);
    uint32_t indexOffset = static_cast<uint32_t>(offset); offset += indexDataSize;
    uint32_t boundsOffset = static_cast<uint32_t>(offset);

    MeshHeader header;
    header.vertexCount = vertexCount;
    header.indexCount = indexCount;
    header.submeshCount = submeshCount;
    header.streamCount = 1u;
    header.lodOffset = lodOffset;
    header.submeshOffset = submeshOffset;
    header.vertexOffset = vertexOffset;
    header.indexOffset = indexOffset;
    header.boundsOffset = boundsOffset;
    AppendBytes(out, header);

    MeshLOD lod;
    lod.indexStart = 0;
    lod.indexCount = indexCount;
    lod.screenSize = 1.f;
    AppendBytes(out, lod);
    while (out.size() < lodOffset + lodTableSize) AppendBytes<uint32_t>(out, 0u);

    for (const auto& s : mesh.submeshes) {
        SubmeshRecord rec{};
        rec.indexStart = s.indexStart;
        rec.indexCount = s.indexCount;
        rec.materialIndex = s.materialIndex;
        AppendBytes(out, rec);
    }
    while (out.size() < submeshOffset + submeshTableSize) AppendBytes<uint32_t>(out, 0u);

    for (const auto& v : mesh.vertices) {
        VertexRecord rec{};
        rec.px = v.position.x; rec.py = v.position.y; rec.pz = v.position.z;
        rec.nx = v.normal.x; rec.ny = v.normal.y; rec.nz = v.normal.z;
        rec.tx = v.tangent.x; rec.ty = v.tangent.y; rec.tz = v.tangent.z; rec.tw = v.tangent.w;
        rec.u0 = v.uv0.x; rec.v0 = v.uv0.y;
        rec.u1 = v.uv1.x; rec.v1 = v.uv1.y;
        AppendBytes(out, rec);
    }
    while (out.size() < vertexOffset + vertexDataSize) AppendBytes<uint32_t>(out, 0u);

    if (!mesh.indices.empty())
        AppendRaw(out, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
    while (out.size() < indexOffset + indexDataSize) AppendBytes<uint32_t>(out, 0u);

    BoundsRecord b{};
    b.centerX = (mesh.bounds.min.x + mesh.bounds.max.x) * 0.5f;
    b.centerY = (mesh.bounds.min.y + mesh.bounds.max.y) * 0.5f;
    b.centerZ = (mesh.bounds.min.z + mesh.bounds.max.z) * 0.5f;
    b.extentsX = (mesh.bounds.max.x - mesh.bounds.min.x) * 0.5f;
    b.extentsY = (mesh.bounds.max.y - mesh.bounds.min.y) * 0.5f;
    b.extentsZ = (mesh.bounds.max.z - mesh.bounds.min.z) * 0.5f;
    b.radius = std::sqrt(b.extentsX*b.extentsX + b.extentsY*b.extentsY + b.extentsZ*b.extentsZ);
    AppendBytes(out, b);
    return true;
}

/** Replace importer-internal keys (textures/...) with dashed GUIDs from \a textureGuidByPath. */
static void FinalizeImportMaterialTextureGuids(std::vector<ImportMaterial>& materials,
                                             const std::unordered_map<std::string, std::string>& textureGuidByPath) {
    auto fix = [&](TextureRef& ref) {
        if (ref.textureAssetId.empty()) return;
        std::string key = ref.textureAssetId;
        std::replace(key.begin(), key.end(), '\\', '/');
        while (!key.empty() && key[0] == '/') key.erase(0, 1);
        if (key.find("textures/") != 0) key = "textures/" + key;
        auto it = textureGuidByPath.find(key);
        if (it != textureGuidByPath.end()) ref.textureAssetId = it->second;
        else ref.textureAssetId.clear();
    };
    for (auto& mat : materials) {
        fix(mat.albedo);
        fix(mat.normal);
        fix(mat.metallic);
        fix(mat.roughness);
    }
}

bool SerializeMaterial(const ImportMaterial& material, std::vector<uint8_t>& out) {
    asset::LoadedMaterial lm;
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
    return asset::MaterialReader::EncodeToBuffer(lm, out);
}

bool SerializeAnimation(const ImportAnimation& animation, std::vector<uint8_t>& out) {
    out.clear();
    const uint32_t magic = 0x414E4931u; // ANI1
    const uint32_t version = 1u;
    const uint32_t nameLen = static_cast<uint32_t>(animation.name.size());
    const uint32_t channelCount = static_cast<uint32_t>(animation.channels.size());
    AppendBytes(out, magic);
    AppendBytes(out, version);
    AppendBytes(out, nameLen);
    AppendBytes(out, channelCount);
    AppendBytes(out, animation.duration);
    if (!animation.name.empty()) AppendRaw(out, animation.name.data(), animation.name.size());
    for (const auto& ch : animation.channels) {
        const uint32_t boneNameLen = static_cast<uint32_t>(ch.boneName.size());
        const int32_t boneIndex = ch.boneIndex;
        const uint32_t posCount = static_cast<uint32_t>(ch.positionKeys.size());
        const uint32_t rotCount = static_cast<uint32_t>(ch.rotationKeys.size());
        const uint32_t sclCount = static_cast<uint32_t>(ch.scaleKeys.size());
        AppendBytes(out, boneNameLen);
        AppendBytes(out, boneIndex);
        AppendBytes(out, posCount);
        AppendBytes(out, rotCount);
        AppendBytes(out, sclCount);
        if (boneNameLen) AppendRaw(out, ch.boneName.data(), ch.boneName.size());
        for (const auto& pk : ch.positionKeys) { AppendBytes(out, pk.time); AppendRaw(out, &pk.value, 12); }
        for (const auto& rk : ch.rotationKeys) { AppendBytes(out, rk.time); AppendRaw(out, &rk.value, 16); }
        for (const auto& sk : ch.scaleKeys) { AppendBytes(out, sk.time); AppendRaw(out, &sk.value, 12); }
    }
    return true;
}

void WriteNodeText(std::ostringstream& out, const ImportScene& scene, int nodeIndex, int depth) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(scene.nodes.size())) return;
    const ImportNode& node = scene.nodes[static_cast<size_t>(nodeIndex)];
    std::string indent(depth * 2, ' ');
    out << indent << "- name: \"" << node.name << "\"\n";
    out << indent << "  transform: [";
    for (int i = 0; i < 16; ++i) { if (i) out << ", "; out << node.localTransform.m[i]; }
    out << "]\n";
    if (node.meshIndex >= 0 && node.meshIndex < static_cast<int>(scene.meshes.size())) {
        out << indent << "  mesh: \"" << scene.meshes[static_cast<size_t>(node.meshIndex)].name << ".mesh\"\n";
        // Stable reference used by InstantiateOmesh; mesh path remains for readability/fallback.
        out << indent << "  meshIndex: " << node.meshIndex << "\n";
    }
    out << indent << "  children:\n";
    for (int c : node.children) WriteNodeText(out, scene, c, depth + 1);
}

bool SerializePrefab(const ImportScene& scene, std::vector<uint8_t>& out) {
    std::ostringstream ss;
    ss << "prefab:\n";
    ss << "  source: \"" << scene.sourcePath << "\"\n";
    ss << "  entities:\n";
    if (scene.rootNodeIndex >= 0) WriteNodeText(ss, scene, scene.rootNodeIndex, 0);
    const std::string s = ss.str();
    out.assign(s.begin(), s.end());
    return true;
}

std::vector<uint8_t> BuildMaterialMapBytes(const ImportScene& scene, bool inMaterialsFolder) {
    std::ostringstream ss;
    ss << "# material_index|material_file\n";
    for (size_t i = 0; i < scene.materials.size(); ++i) {
        if (inMaterialsFolder) ss << i << "|materials/";
        else ss << i << "|";
        ss << scene.materials[i].name << ".mat\n";
    }
    const std::string s = ss.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

bool WriteFileBytes(const fs::path& p, const std::vector<uint8_t>& bytes) {
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p.string(), std::ios::binary);
    if (!f) return false;
    if (!bytes.empty() && !f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        return false;
    return f.good();
}

/** Write .meta next to asset; returns the assigned GUID. */
std::string WriteMetaForAsset(const fs::path& assetPath, const std::string& type,
                              const std::vector<std::string>& dependencies = {}) {
    MetaFile meta;
    meta.guid = GenerateGuidString();
    meta.type = type;
    meta.dependencies = dependencies;
    meta.importer = "model";
    meta.version = 1;
    const std::string metaPath = assetPath.string() + ".meta";
    WriteMetaFile(metaPath, meta);
    return meta.guid;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << (argc ? argv[0] : "importer") << " <source_file> [output_dir] [--mat | --oea]\n";
        std::cerr << "  default: model/<name>.omesh + textures/ + anims (materials embedded in .omesh)\n";
        std::cerr << "  --mat  : model/<name>.omesh + materials/*.mat + textures/ + anims\n";
        std::cerr << "  --oea  : single <name>.oea only (no loose writes)\n";
        return 1;
    }

    const std::string sourcePath = argv[1];
    std::string outputDir = fs::path(sourcePath).parent_path().string();
    OutputMode mode = OutputMode::OmeshEmbeddedMaterials;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--oea") mode = OutputMode::OeaOnly;
        else if (arg == "--mat") mode = OutputMode::OmeshSplitMaterials;
        else if (!arg.empty()) outputDir = arg;
    }
    if (outputDir.empty()) outputDir = ".";

    Log(("Loading " + sourcePath).c_str());
    SceneLoader loader;
    const aiScene* scene = loader.Load(sourcePath.c_str());
    if (!scene) {
        std::cerr << "[Importer] Error: " << loader.GetError() << std::endl;
        return 2;
    }

    ImportScene importScene;
    if (!SceneConverter::Convert(scene, sourcePath.c_str(), importScene)) {
        std::cerr << "[Importer] Convert failed\n";
        return 3;
    }

    fs::create_directories(outputDir);
    fs::path out(outputDir);

    Log(("Meshes: " + std::to_string(importScene.meshes.size())).c_str());
    Log(("Materials: " + std::to_string(importScene.materials.size())).c_str());
    Log(("Textures: " + std::to_string(importScene.textures.size())).c_str());
    Log(("Animations: " + std::to_string(importScene.animations.size())).c_str());

    // Build in-memory entries shared by .omesh/.oea container writing.
    std::vector<OeaInputEntry> meshEntries;
    meshEntries.reserve(importScene.meshes.size() + 2);
    for (const auto& m : importScene.meshes) {
        std::vector<uint8_t> payload;
        if (!SerializeMesh(m, payload)) return 4;
        meshEntries.push_back({OeaFormat::Entry_Mesh, m.name + ".mesh", "", std::move(payload)});
    }
    {
        std::vector<uint8_t> prefabPayload;
        if (!SerializePrefab(importScene, prefabPayload)) return 4;
        meshEntries.push_back({OeaFormat::Entry_Prefab, importScene.sourceName + ".prefab", "", std::move(prefabPayload)});
    }
    meshEntries.push_back({OeaFormat::Entry_MaterialMap, importScene.sourceName + ".materialmap", "",
                           BuildMaterialMapBytes(importScene, mode == OutputMode::OmeshSplitMaterials)});

    if (mode == OutputMode::OeaOnly) {
        std::unordered_map<std::string, std::string> oeaTextureGuidByPath;
        for (const auto& tex : importScene.textures) {
            if (tex.encodedData.empty()) continue;
            const std::string n = tex.outputFileName.empty() ? (tex.name + ".png") : tex.outputFileName;
            oeaTextureGuidByPath["textures/" + n] = asset::GenerateGuidString();
        }
        FinalizeImportMaterialTextureGuids(importScene.materials, oeaTextureGuidByPath);

        std::vector<OeaInputEntry> packageEntries = meshEntries;
        for (const auto& mat : importScene.materials) {
            std::vector<uint8_t> payload;
            if (!SerializeMaterial(mat, payload)) return 4;
            packageEntries.push_back({OeaFormat::Entry_Material, mat.name + ".mat", "", std::move(payload)});
        }
        for (const auto& tex : importScene.textures) {
            if (!tex.encodedData.empty()) {
                const std::string n = tex.outputFileName.empty() ? (tex.name + ".png") : tex.outputFileName;
                packageEntries.push_back({OeaFormat::Entry_Texture, "textures/" + n, "", tex.encodedData});
            }
        }
        for (const auto& anim : importScene.animations) {
            std::vector<uint8_t> payload;
            if (!SerializeAnimation(anim, payload)) return 4;
            packageEntries.push_back({OeaFormat::Entry_Animation, anim.name + ".anim", "", std::move(payload)});
        }
        const std::string oeaPath = (out / (importScene.sourceName + ".oea")).string();
        if (!OeaWriter::Write(oeaPath, packageEntries)) {
            std::cerr << "[Importer] Failed to write " << oeaPath << "\n";
            return 5;
        }
        WriteMetaForAsset(fs::path(oeaPath), "Package", {});
        Log(("Writing " + oeaPath).c_str());
        Log("Done.");
        return 0;
    }

    const fs::path modelDir = out / importScene.sourceName;
    const fs::path texturesDir = modelDir / "textures";
    std::error_code ec;
    fs::create_directories(texturesDir, ec);
    if (ec) {
        std::cerr << "[Importer] Failed to create texture directory: " << texturesDir.string() << "\n";
        return 6;
    }

    // Path (relative to model dir) -> GUID for dependency resolution.
    std::unordered_map<std::string, std::string> textureGuidByPath;
    std::vector<std::string> materialGuids;

    // Loose textures (original extension/encoded bytes).
    for (const auto& tex : importScene.textures) {
        const std::string n = tex.outputFileName.empty() ? (tex.name + ".png") : tex.outputFileName;
        if (!tex.encodedData.empty()) {
            const fs::path p = texturesDir / n;
            if (!WriteFileBytes(p, tex.encodedData)) {
                std::cerr << "[Importer] Failed to write " << p.string() << "\n";
                return 7;
            }
            const std::string relPath = ("textures/" + n);
            textureGuidByPath[relPath] = WriteMetaForAsset(p, "Texture", {});
            Log(("Writing " + p.string()).c_str());
        }
    }

    FinalizeImportMaterialTextureGuids(importScene.materials, textureGuidByPath);

    // Loose animations in model folder.
    for (const auto& anim : importScene.animations) {
        const fs::path p = modelDir / (anim.name + ".anim");
        fs::create_directories(modelDir, ec);
        if (!AnimationWriter::Write(anim, p.string())) {
            std::cerr << "[Importer] Failed to write " << p.string() << "\n";
            return 8;
        }
        WriteMetaForAsset(p, "Animation", {});
        Log(("Writing " + p.string()).c_str());
    }

    if (mode == OutputMode::OmeshSplitMaterials) {
        const fs::path matDir = modelDir / "materials";
        fs::create_directories(matDir, ec);
        if (ec) return 9;
        for (const auto& mat : importScene.materials) {
            std::vector<std::string> deps;
            auto addDep = [&](const TextureRef& ref) {
                if (!ref.textureAssetId.empty()) deps.push_back(ref.textureAssetId);
            };
            addDep(mat.albedo);
            addDep(mat.normal);
            addDep(mat.metallic);
            addDep(mat.roughness);
            const fs::path p = matDir / (mat.name + ".mat");
            if (!MaterialWriter::Write(mat, p.string())) {
                std::cerr << "[Importer] Failed to write " << p.string() << "\n";
                return 10;
            }
            materialGuids.push_back(WriteMetaForAsset(p, "Material", deps));
            Log(("Writing " + p.string()).c_str());
        }
    } else {
        for (const auto& mat : importScene.materials) {
            std::vector<uint8_t> payload;
            if (!SerializeMaterial(mat, payload)) return 11;
            meshEntries.push_back({OeaFormat::Entry_Material, mat.name + ".mat", "", std::move(payload)});
        }
    }

    const std::string omeshPath = (modelDir / (importScene.sourceName + ".omesh")).string();
    if (!OeaWriter::Write(omeshPath, meshEntries)) {
        std::cerr << "[Importer] Failed to write " << omeshPath << "\n";
        return 12;
    }
    WriteMetaForAsset(fs::path(omeshPath), "Mesh", materialGuids);
    Log(("Writing " + omeshPath).c_str());

    Log("Done.");
    return 0;
}
