#include "api/asset/AssetRegistry.h"
#include "api/asset/IGPUResourceResolver.h"
#include "asset/core/guid.h"
#include "asset/core/asset_type.h"
#include "asset/database/asset_database.h"
#include "asset/runtime/asset_manager.h"
#include "asset/readers/material_reader.h"
#include "asset/readers/texture_reader.h"
#include "asset/core/import_mesh.h"
#include "eyelib/render/MaterialDescriptor.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/TextureDescriptor.h"
#include "eyelib/render/ShaderDescriptor.h"
#include "eyelib/math/Types.h"
#include "eyelib/render/ShaderLoader.h"
#include "api/render/Material.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>

namespace eyelib {
namespace api {

namespace {

asset::Guid ToGuid(const AssetId& id) {
    asset::Guid g;
    g.high = id.high;
    g.low = id.low;
    return g;
}

AssetId ToApi(const asset::Guid& g) {
    return AssetId{ g.high, g.low };
}

AssetType ToApiType(asset::AssetType t) {
    switch (t) {
        case asset::AssetType::Mesh: return AssetType::Mesh;
        case asset::AssetType::Material: return AssetType::Material;
        case asset::AssetType::Texture: return AssetType::Texture;
        case asset::AssetType::Animation: return AssetType::Animation;
        case asset::AssetType::Package: return AssetType::Prefab;
        default: return AssetType::Unknown;
    }
}

struct AssetIdHash {
    size_t operator()(const AssetId& id) const {
        return id.high ^ (id.low << 1);
    }
};

void LoadedMaterialToDescriptor(const asset::LoadedMaterial& src, eyelib::render::MaterialDescriptor& dst) {
    dst.shaderName = "lit";
    dst.albedo.x = src.albedoColorR;
    dst.albedo.y = src.albedoColorG;
    dst.albedo.z = src.albedoColorB;
    dst.metallic = src.metallicValue;
    dst.roughness = src.roughnessValue;
    if (!src.albedoPath.empty()) dst.texturePaths["albedo"] = src.albedoPath;
    if (!src.normalPath.empty()) dst.texturePaths["normal"] = src.normalPath;
    if (!src.metallicPath.empty()) dst.texturePaths["metallic"] = src.metallicPath;
    if (!src.roughnessPath.empty()) dst.texturePaths["roughness"] = src.roughnessPath;
}

void ImportMeshToDescriptor(const asset::ImportMesh& src, eyelib::render::MeshDescriptor& dst) {
    dst.vertices.resize(src.vertices.size());
    for (size_t i = 0; i < src.vertices.size(); ++i) {
        const auto& v = src.vertices[i];
        auto& out = dst.vertices[i];
        out.position.x = v.position.x;
        out.position.y = v.position.y;
        out.position.z = v.position.z;
        out.normal.x = v.normal.x;
        out.normal.y = v.normal.y;
        out.normal.z = v.normal.z;
        out.uv.x = v.uv0.x;
        out.uv.y = v.uv0.y;
        out.uv1.x = v.uv1.x;
        out.uv1.y = v.uv1.y;
        out.tangent.x = v.tangent.x;
        out.tangent.y = v.tangent.y;
        out.tangent.z = v.tangent.z;
    }
    dst.indices32 = src.indices;
    dst.indexFormat = eyelib::render::IndexFormat::UInt32;
    dst.submeshes.resize(src.submeshes.size());
    for (size_t i = 0; i < src.submeshes.size(); ++i) {
        dst.submeshes[i].indexOffset = src.submeshes[i].indexStart;
        dst.submeshes[i].indexCount = src.submeshes[i].indexCount;
        dst.submeshes[i].materialIndex = static_cast<int>(src.submeshes[i].materialIndex);
    }
}

std::string NormalizePathForDb(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    while (path.size() > 1 && path[0] == '/') path.erase(0, 1);
    return path;
}

} // namespace

class AssetRegistry::Impl {
public:
    asset::AssetDatabase database;
    std::string assetRoot;
    void* renderDevice = nullptr;
    bool readyForResolution = false;
    IGPUResourceResolver* resolver = nullptr;

    mutable std::unordered_map<AssetId, eyelib::render::MaterialDescriptor, AssetIdHash> materialDescriptors;
    mutable std::unordered_map<AssetId, eyelib::render::MeshDescriptor, AssetIdHash> meshDescriptors;
    mutable std::unordered_map<AssetId, eyelib::render::TextureDescriptor, AssetIdHash> textureDescriptors;
    mutable std::unordered_map<AssetId, eyelib::render::ShaderDescriptor, AssetIdHash> shaderDescriptors;

    void SyncToAssetManager() {
        asset::AssetManager::Get().SetDatabase(&database);
        asset::AssetManager::Get().SetAssetRoot(assetRoot);
    }
};

AssetRegistry::AssetRegistry() : m_impl(new Impl()) {}

AssetRegistry::~AssetRegistry() {
    delete m_impl;
    m_impl = nullptr;
}

void AssetRegistry::SetAssetRoot(const std::string& root) {
    m_impl->assetRoot = root;
    m_impl->SyncToAssetManager();
}

std::string AssetRegistry::GetAssetRoot() const {
    return m_impl->assetRoot;
}

bool AssetRegistry::RegisterAsset(const AssetId& id, const std::string& filePath, const std::string& assetType) {
    std::string rel = NormalizePathForDb(filePath);
    asset::Guid g = ToGuid(id);
    if (m_impl->database.AssetExists(g)) return true;
    return m_impl->database.AddRuntimeAsset(rel, assetType, g);
}

AssetId AssetRegistry::RegisterAsset(const std::string& filePath, const std::string& assetType) {
    std::string rel = NormalizePathForDb(filePath);
    asset::Guid g = m_impl->database.GetGuid(rel);
    if (!g.IsValid()) g = m_impl->database.AddRuntimeAsset(rel, assetType);
    return ToApi(g);
}

AssetId AssetRegistry::LoadAsset(const std::string& filePath, AssetType type) {
    AssetId id = RegisterAsset(filePath, ToString(type));
    if (!id.IsValid()) return id;
    asset::Guid g = ToGuid(id);
    switch (type) {
        case AssetType::Mesh: asset::AssetManager::Get().LoadMesh(g); break;
        case AssetType::Material: asset::AssetManager::Get().LoadMaterial(g); break;
        case AssetType::Texture: asset::AssetManager::Get().LoadTexture(g); break;
        case AssetType::Animation: asset::AssetManager::Get().LoadAnimation(g); break;
        case AssetType::Shader: {
            eyelib::render::ShaderDescriptor desc;
            if (filePath.size() > 5 && filePath.compare(0, 5, "name:") == 0)
                desc = eyelib::render::ShaderLoader::BuildDescriptorByName(filePath.substr(5));
            else {
                std::string::size_type sep = filePath.find('|');
                if (sep != std::string::npos)
                    desc = eyelib::render::ShaderLoader::BuildDescriptorFromPaths(filePath.substr(0, sep), filePath.substr(sep + 1));
                else
                    desc = eyelib::render::ShaderLoader::BuildDescriptorFromPaths(filePath, filePath);
            }
            m_impl->shaderDescriptors[id] = std::move(desc);
            break;
        }
        default: break;
    }
    return id;
}

AssetId AssetRegistry::LoadAssetDescriptorOnly(const std::string& filePath, AssetType type) {
    return LoadAsset(filePath, type);
}

void AssetRegistry::UnloadAsset(const AssetId& id) {
    asset::AssetManager::Get().UnloadAsset(ToGuid(id));
    m_impl->materialDescriptors.erase(id);
    m_impl->meshDescriptors.erase(id);
    m_impl->textureDescriptors.erase(id);
    m_impl->shaderDescriptors.erase(id);
}

bool AssetRegistry::ReloadAsset(const AssetId& id) {
    asset::Guid g = ToGuid(id);
    if (!m_impl->database.AssetExists(g)) return false;
    const asset::AssetInfo* info = m_impl->database.GetAssetInfo(g);
    if (!info) return false;
    AssetType type = ToApiType(info->type);
    UnloadAsset(id);
    LoadAsset(m_impl->assetRoot + "/" + info->path, type);
    return true;
}

bool AssetRegistry::IsRegistered(const AssetId& id) const {
    return m_impl->database.AssetExists(ToGuid(id));
}

bool AssetRegistry::IsRegistered(const std::string& filePath) const {
    return m_impl->database.GetGuid(NormalizePathForDb(filePath)).IsValid();
}

AssetId AssetRegistry::GetId(const std::string& filePath) const {
    return ToApi(m_impl->database.GetGuid(NormalizePathForDb(filePath)));
}

std::string AssetRegistry::GetFilePath(const AssetId& id) const {
    std::string rel = m_impl->database.GetPath(ToGuid(id));
    if (rel.empty()) return {};
    if (m_impl->assetRoot.empty()) return rel;
    return m_impl->assetRoot + "/" + rel;
}

std::string AssetRegistry::GetAssetPath(const AssetId& id) const {
    return GetFilePath(id);
}

AssetType AssetRegistry::GetAssetType(const AssetId& id) const {
    const asset::AssetInfo* info = m_impl->database.GetAssetInfo(ToGuid(id));
    return info ? ToApiType(info->type) : AssetType::Unknown;
}

size_t AssetRegistry::GetAssetSize(const AssetId& id) const {
    const asset::AssetInfo* info = m_impl->database.GetAssetInfo(ToGuid(id));
    return info ? static_cast<size_t>(info->fileSize) : 0;
}

size_t AssetRegistry::GetAssetMemoryUsage() const {
    return 0;
}

std::shared_ptr<AnimationClip> AssetRegistry::GetAnimationClip(const AssetId& id, size_t index) const {
    (void)id;
    (void)index;
    return nullptr;
}

std::shared_ptr<AnimationClip> AssetRegistry::GetAnimationClip(const std::string& path, size_t index) const {
    return GetAnimationClip(GetId(path), index);
}

std::shared_ptr<Skeleton> AssetRegistry::GetSkeleton(const AssetId& id) const {
    (void)id;
    return nullptr;
}

std::shared_ptr<Skeleton> AssetRegistry::GetSkeleton(const std::string& path) const {
    return GetSkeleton(GetId(path));
}

std::shared_ptr<Material> AssetRegistry::GetMaterial(const AssetId& id) const {
    if (!id.IsValid()) return nullptr;
    if (m_impl->resolver) {
        auto internal = m_impl->resolver->GetMaterial(id);
        return internal ? Material::WrapInternal(internal) : nullptr;
    }
    return nullptr;
}

std::shared_ptr<Material> AssetRegistry::GetMaterial(const std::string& path) const {
    return GetMaterial(GetId(path));
}

void AssetRegistry::AddDependency(const AssetId& asset, const AssetId& dependency) {
    (void)asset;
    (void)dependency;
}

void AssetRegistry::RemoveDependency(const AssetId& asset, const AssetId& dependency) {
    (void)asset;
    (void)dependency;
}

std::vector<AssetId> AssetRegistry::GetDependencies(const AssetId& id) const {
    auto deps = m_impl->database.GetDependencies(ToGuid(id));
    std::vector<AssetId> out;
    out.reserve(deps.size());
    for (const auto& d : deps) out.push_back(ToApi(d));
    return out;
}

std::vector<AssetId> AssetRegistry::GetDependents(const AssetId& id) const {
    auto deps = m_impl->database.GetDependents(ToGuid(id));
    std::vector<AssetId> out;
    out.reserve(deps.size());
    for (const auto& d : deps) out.push_back(ToApi(d));
    return out;
}

void AssetRegistry::SetRenderDevice(RenderDevice device) {
    m_impl->renderDevice = device.Get();
    m_impl->readyForResolution = (m_impl->renderDevice != nullptr);
}

RenderDevice AssetRegistry::GetRenderDevice() const {
    return RenderDevice(m_impl->renderDevice);
}

bool AssetRegistry::IsReadyForResolution() const {
    return m_impl->readyForResolution;
}

void AssetRegistry::SetGPUResolver(IGPUResourceResolver* resolver) {
    m_impl->resolver = resolver;
}

IGPUResourceResolver* AssetRegistry::GetGPUResolver() const {
    return m_impl->resolver;
}

const eyelib::render::MaterialDescriptor* AssetRegistry::GetMaterialDescriptor(const AssetId& id) const {
    if (!id.IsValid()) return nullptr;
    auto it = m_impl->materialDescriptors.find(id);
    if (it != m_impl->materialDescriptors.end()) return &it->second;
    auto loaded = asset::AssetManager::Get().LoadMaterial(ToGuid(id));
    if (!loaded) return nullptr;
    LoadedMaterialToDescriptor(*loaded, m_impl->materialDescriptors[id]);
    return &m_impl->materialDescriptors[id];
}

const eyelib::render::MeshDescriptor* AssetRegistry::GetMeshDescriptor(const AssetId& id) const {
    if (!id.IsValid()) return nullptr;
    auto it = m_impl->meshDescriptors.find(id);
    if (it != m_impl->meshDescriptors.end()) return &it->second;
    auto loaded = asset::AssetManager::Get().LoadMesh(ToGuid(id));
    if (!loaded) return nullptr;
    ImportMeshToDescriptor(*loaded, m_impl->meshDescriptors[id]);
    return &m_impl->meshDescriptors[id];
}

const eyelib::render::TextureDescriptor* AssetRegistry::GetTextureDescriptor(const AssetId& id) const {
    if (!id.IsValid()) return nullptr;
    auto it = m_impl->textureDescriptors.find(id);
    if (it != m_impl->textureDescriptors.end()) return &it->second;
    std::string rel = m_impl->database.GetPath(ToGuid(id));
    if (rel.empty()) return nullptr;
    eyelib::render::TextureDescriptor& d = m_impl->textureDescriptors[id];
    d.filePath = m_impl->assetRoot.empty() ? rel : (m_impl->assetRoot + "/" + rel);
    return &m_impl->textureDescriptors[id];
}

const eyelib::render::ShaderDescriptor* AssetRegistry::GetShaderDescriptor(const AssetId& id) const {
    if (!id.IsValid()) return nullptr;
    auto it = m_impl->shaderDescriptors.find(id);
    if (it != m_impl->shaderDescriptors.end()) return &it->second;
    std::string rel = m_impl->database.GetPath(ToGuid(id));
    if (rel.empty()) return nullptr;
    eyelib::render::ShaderDescriptor& d = m_impl->shaderDescriptors[id];
    d.vertexPath = m_impl->assetRoot.empty() ? rel : (m_impl->assetRoot + "/" + rel);
    d.fragmentPath = d.vertexPath;
    return &m_impl->shaderDescriptors[id];
}

void AssetRegistry::OnDeviceLost() {
    m_impl->readyForResolution = false;
    if (m_impl->resolver) m_impl->resolver->OnDeviceLost();
}

bool AssetRegistry::SaveToFile(const std::string& filepath) const {
    return m_impl->database.SaveCache(filepath);
}

bool AssetRegistry::LoadFromFile(const std::string& filepath) {
    bool ok = m_impl->database.LoadCache(filepath, true);
    if (ok) m_impl->SyncToAssetManager();
    return ok;
}

void AssetRegistry::Scan() {
    if (!m_impl->assetRoot.empty()) {
        m_impl->database.Scan(m_impl->assetRoot);
        m_impl->SyncToAssetManager();
    }
}

void AssetRegistry::Clear() {
    m_impl->database.Clear();
    m_impl->materialDescriptors.clear();
    m_impl->meshDescriptors.clear();
    m_impl->textureDescriptors.clear();
    m_impl->shaderDescriptors.clear();
    asset::AssetManager::Get().ClearCache();
    m_impl->SyncToAssetManager();
}

} // namespace api
} // namespace eyelib
