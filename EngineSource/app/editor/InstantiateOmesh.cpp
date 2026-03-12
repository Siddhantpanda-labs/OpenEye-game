#include "InstantiateOmesh.h"
#include "EditorContext.h"
#include "api/world/Commands.h"
#include "api/math/TransformUtils.h"
#include "api/platform/All.h"
#include "eyelib/asset/readers/oea_reader.h"
#include "eyelib/asset/readers/mesh_reader.h"
#include "eyelib/asset/readers/prefab_reader.h"
#include "eyelib/asset/core/oea_format.h"
#include "eyelib/asset/core/import_mesh.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_map>

namespace openeye::editor {

namespace fs = eyelib::api::platform::filesystem;

std::string GetGuidFromMetaPath(const std::string& assetPath) {
    std::string path = assetPath + ".meta";
    std::ifstream f(path);
    if (!f) return {};
    std::string line;
    while (std::getline(f, line)) {
        auto start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        if (line.size() - start >= 5 && line.compare(start, 5, "guid:") == 0) {
            start = line.find_first_not_of(" \t", start + 5);
            if (start == std::string::npos) return {};
            std::string guid = line.substr(start);
            auto end = guid.find_last_not_of(" \t\r\n");
            if (end != std::string::npos)
                guid = guid.substr(0, end + 1);
            return guid;
        }
    }
    return {};
}

namespace {

std::string Trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool ParseMaterialMapText(const std::string& text, std::vector<std::string>& outIndexedFiles) {
    outIndexedFiles.clear();
    bool any = false;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        const size_t sep = line.find('|');
        if (sep == std::string::npos) continue;

        const std::string idxStr = Trim(line.substr(0, sep));
        const std::string materialFile = Trim(line.substr(sep + 1));
        if (idxStr.empty() || materialFile.empty()) continue;

        char* endPtr = nullptr;
        const unsigned long idxValue = std::strtoul(idxStr.c_str(), &endPtr, 10);
        if (endPtr == idxStr.c_str() || *endPtr != '\0') continue;
        const size_t index = static_cast<size_t>(idxValue);
        if (index >= outIndexedFiles.size()) outIndexedFiles.resize(index + 1u);
        outIndexedFiles[index] = materialFile;
        any = true;
    }
    return any;
}

bool ReadMaterialMapFromOmesh(const std::string& omeshPath,
                              const std::vector<asset::OeaEntry>& entries,
                              std::vector<std::string>& outIndexedFiles) {
    outIndexedFiles.clear();
    std::string stem = fs::GetFilename(omeshPath);
    size_t dot = stem.rfind('.');
    if (dot != std::string::npos) stem = stem.substr(0, dot);

    std::string materialMapEntry;
    const std::string preferred = stem + ".materialmap";
    for (const auto& e : entries) {
        if (e.name == preferred) {
            materialMapEntry = e.name;
            break;
        }
    }
    if (materialMapEntry.empty()) {
        for (const auto& e : entries) {
            if (fs::GetExtension(e.name) == ".materialmap") {
                materialMapEntry = e.name;
                break;
            }
        }
    }

    std::vector<uint8_t> mapBytes;
    if (!materialMapEntry.empty() && asset::OeaReader::ReadEntryData(omeshPath, materialMapEntry, mapBytes)) {
        const std::string mapText(mapBytes.begin(), mapBytes.end());
        if (ParseMaterialMapText(mapText, outIndexedFiles))
            return true;
    }

    const std::string mapPath = fs::NormalizePath(fs::GetDirectory(omeshPath) + "/" + stem + ".materialmap");
    if (!fs::FileExists(mapPath)) return false;
    std::ifstream f(mapPath);
    if (!f) return false;
    std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return ParseMaterialMapText(text, outIndexedFiles);
}

std::unordered_map<std::string, int> BuildMeshPathToSubmeshStart(const std::string& omeshPath,
                                                                  const std::vector<asset::OeaEntry>& entries) {
    std::unordered_map<std::string, int> out;
    int running = 0;
    for (const auto& e : entries) {
        if (e.type != asset::OeaFormat::Entry_Mesh) continue;
        out[e.name] = running;
        std::vector<uint8_t> data;
        asset::ImportMesh part;
        if (asset::OeaReader::ReadEntryData(omeshPath, e.name, data) && asset::MeshReader::Read(data, part) && !part.submeshes.empty())
            running += static_cast<int>(part.submeshes.size());
        else
            running += 1;
    }
    return out;
}

std::vector<std::string> BuildMaterialIdsBySubmeshIndex(const std::string& omeshPath,
                                                        const std::vector<asset::OeaEntry>& entries) {
    std::vector<std::string> materialMap;
    ReadMaterialMapFromOmesh(omeshPath, entries, materialMap);

    std::vector<std::string> out;
    for (const auto& e : entries) {
        if (e.type != asset::OeaFormat::Entry_Mesh) continue;
        std::vector<uint8_t> data;
        asset::ImportMesh part;
        if (asset::OeaReader::ReadEntryData(omeshPath, e.name, data) && asset::MeshReader::Read(data, part) && !part.submeshes.empty()) {
            for (const auto& s : part.submeshes) {
                std::string materialId;
                const int matIndex = static_cast<int>(s.materialIndex);
                if (matIndex >= 0 && static_cast<size_t>(matIndex) < materialMap.size() && !materialMap[static_cast<size_t>(matIndex)].empty()) {
                    const std::string materialPath = fs::NormalizePath(fs::GetDirectory(omeshPath) + "/" + materialMap[static_cast<size_t>(matIndex)]);
                    if (fs::FileExists(materialPath)) {
                        std::string matGuid = GetGuidFromMetaPath(materialPath);
                        materialId = matGuid.empty() ? materialPath : matGuid;
                    }
                }
                out.push_back(materialId);
            }
        } else {
            out.push_back(std::string{});
        }
    }
    return out;
}

int MeshPathToSubmeshStart(const std::string& meshPath,
                           const std::unordered_map<std::string, int>& meshPathToSubmeshStart) {
    auto it = meshPathToSubmeshStart.find(meshPath);
    if (it != meshPathToSubmeshStart.end())
        return it->second;
    return -1;
}

void AssignMeshIndicesRecurse(asset::PrefabNode& node,
                              const std::unordered_map<std::string, int>& meshPathToSubmeshStart) {
    if (!node.meshPath.empty())
        node.meshIndex = MeshPathToSubmeshStart(node.meshPath, meshPathToSubmeshStart);
    for (auto& child : node.children)
        AssignMeshIndicesRecurse(child, meshPathToSubmeshStart);
}

void AssignMeshIndices(std::vector<asset::PrefabNode>& roots,
                       const std::unordered_map<std::string, int>& meshPathToSubmeshStart) {
    for (auto& root : roots)
        AssignMeshIndicesRecurse(root, meshPathToSubmeshStart);
}

eyelib::math::Mat4 RowMajorToMat4(const float m[16]) {
    eyelib::math::Mat4 out;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            out[c][r] = m[r * 4 + c];
    return out;
}

::eyelib::world::EntityId CreateEntityFromNode(
    eyelib::api::world::World& world,
    const asset::PrefabNode& node,
    const std::string& meshAssetId,
    const std::vector<std::string>& materialIdsByMeshIndex,
    ::eyelib::world::EntityId parent) {
    auto result = world.ApplyCommandSync(eyelib::api::world::CreateEntity(node.name.empty() ? "Node" : node.name));
    if (!result.success || !result.createdEntity) return ::eyelib::world::InvalidEntity;
    ::eyelib::world::EntityId id = *result.createdEntity;

    if (parent != ::eyelib::world::InvalidEntity && world.IsAlive(parent))
        world.SetParent(id, parent);

    eyelib::math::Vec3 pos, scale;
    eyelib::math::Quat rot;
    eyelib::math::Decompose(RowMajorToMat4(node.localTransform), pos, rot, scale);
    eyelib::world::components::Transform tr;
    tr.position = eyelib::math::SanitizePosition(pos);
    tr.rotation = eyelib::math::SanitizeRotation(rot);
    tr.scale = eyelib::math::SanitizeScale(scale);
    world.ApplyCommandSync(eyelib::api::world::SetTransform(id, tr));

    if (node.meshIndex >= 0 && !meshAssetId.empty()) {
        eyelib::world::components::Renderable r;
        r.meshAssetId = meshAssetId;
        r.submeshIndex = node.meshIndex;
        if (static_cast<size_t>(node.meshIndex) < materialIdsByMeshIndex.size())
            r.materialAssetId = materialIdsByMeshIndex[static_cast<size_t>(node.meshIndex)];
        r.visible = true;
        world.ApplyCommandSync(eyelib::api::world::AddComponent<eyelib::world::components::Renderable>(id, r));
    }

    for (const auto& child : node.children) {
        CreateEntityFromNode(world, child, meshAssetId, materialIdsByMeshIndex, id);
    }
    return id;
}

} // namespace

::eyelib::world::EntityId InstantiateOmesh(
    eyelib::api::world::World& world,
    const std::string& omeshPath,
    const std::string& meshAssetId,
    ::eyelib::world::EntityId parent) {
    if (omeshPath.size() < 7 || meshAssetId.empty()) return ::eyelib::world::InvalidEntity;
    std::string ext = omeshPath.substr(omeshPath.size() - 6);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext != ".omesh") return ::eyelib::world::InvalidEntity;

    std::vector<asset::OeaEntry> entries;
    if (!asset::OeaReader::ReadIndex(omeshPath, entries)) return ::eyelib::world::InvalidEntity;

    std::string stem = fs::GetFilename(omeshPath);
    size_t dot = stem.rfind('.');
    if (dot != std::string::npos) stem = stem.substr(0, dot);
    std::string prefabEntryName = stem + ".prefab";
    bool found = false;
    for (const auto& e : entries) {
        if (e.type == asset::OeaFormat::Entry_Prefab && e.name == prefabEntryName) {
            found = true;
            break;
        }
    }
    if (!found) {
        for (const auto& e : entries) {
            if (e.type == asset::OeaFormat::Entry_Prefab) {
                prefabEntryName = e.name;
                found = true;
                break;
            }
        }
    }
    if (!found) return ::eyelib::world::InvalidEntity;

    std::vector<uint8_t> prefabBytes;
    if (!asset::OeaReader::ReadEntryData(omeshPath, prefabEntryName, prefabBytes)) return ::eyelib::world::InvalidEntity;
    std::string prefabText(prefabBytes.begin(), prefabBytes.end());

    std::vector<asset::PrefabNode> roots;
    if (!asset::PrefabReader::ReadHierarchyFromText(prefabText, roots)) return ::eyelib::world::InvalidEntity;

    const auto meshPathToSubmeshStart = BuildMeshPathToSubmeshStart(omeshPath, entries);
    AssignMeshIndices(roots, meshPathToSubmeshStart);
    const std::vector<std::string> materialIdsByMeshIndex = BuildMaterialIdsBySubmeshIndex(omeshPath, entries);

    ::eyelib::world::EntityId rootEntity = ::eyelib::world::InvalidEntity;
    for (size_t i = 0; i < roots.size(); ++i) {
        ::eyelib::world::EntityId nodeRoot = CreateEntityFromNode(world, roots[i], meshAssetId, materialIdsByMeshIndex, parent);
        if (i == 0) rootEntity = nodeRoot;
    }
    return rootEntity;
}

} // namespace openeye::editor
