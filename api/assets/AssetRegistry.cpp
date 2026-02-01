#include "api/assets/AssetRegistry.h"

#include "eyelib/assets/AssetRegistry.h"
#include "eyelib/assets/AssetTypes.h"
#include "eyelib/assets/AnimationAsset.h"
#include "eyelib/render/resources/Material.h"

namespace eyelib {
namespace api {

static eyelib::assets::AssetId ToCore(const AssetId& id) {
    return eyelib::assets::AssetId(id.high, id.low);
}

static AssetId ToApi(const eyelib::assets::AssetId& id) {
    return AssetId{ id.High(), id.Low() };
}

static eyelib::assets::AssetType ToCoreType(AssetType type) {
    using Core = eyelib::assets::AssetType;
    switch (type) {
        case AssetType::Mesh: return Core::Mesh;
        case AssetType::Texture: return Core::Texture;
        case AssetType::Material: return Core::Material;
        case AssetType::Shader: return Core::Shader;
        case AssetType::Audio: return Core::Audio;
        case AssetType::Animation: return Core::Animation;
        case AssetType::Prefab: return Core::Prefab;
        case AssetType::Scene: return Core::Scene;
        default: return Core::Unknown;
    }
}

static AssetType ToApiType(eyelib::assets::AssetType type) {
    using Core = eyelib::assets::AssetType;
    switch (type) {
        case Core::Mesh: return AssetType::Mesh;
        case Core::Texture: return AssetType::Texture;
        case Core::Material: return AssetType::Material;
        case Core::Shader: return AssetType::Shader;
        case Core::Audio: return AssetType::Audio;
        case Core::Animation: return AssetType::Animation;
        case Core::Prefab: return AssetType::Prefab;
        case Core::Scene: return AssetType::Scene;
        default: return AssetType::Unknown;
    }
}

class AssetRegistry::Impl {
public:
    eyelib::assets::AssetRegistry reg{};
};

AssetRegistry::AssetRegistry() : m_impl(new Impl()) {}

AssetRegistry::~AssetRegistry() {
    delete m_impl;
    m_impl = nullptr;
}

bool AssetRegistry::RegisterAsset(const AssetId& id, const std::string& filePath, const std::string& assetType) {
    return m_impl->reg.RegisterAsset(ToCore(id), filePath, assetType);
}

AssetId AssetRegistry::RegisterAsset(const std::string& filePath, const std::string& assetType) {
    return ToApi(m_impl->reg.RegisterAsset(filePath, assetType));
}

AssetId AssetRegistry::LoadAsset(const std::string& filePath, AssetType type) {
    return ToApi(m_impl->reg.LoadAsset(filePath, ToCoreType(type)));
}

void AssetRegistry::UnloadAsset(const AssetId& id) {
    m_impl->reg.UnloadAsset(ToCore(id));
}

bool AssetRegistry::ReloadAsset(const AssetId& id) {
    return m_impl->reg.ReloadAsset(ToCore(id));
}

bool AssetRegistry::IsRegistered(const AssetId& id) const {
    return m_impl->reg.IsRegistered(ToCore(id));
}

bool AssetRegistry::IsRegistered(const std::string& filePath) const {
    return m_impl->reg.IsRegistered(filePath);
}

AssetId AssetRegistry::GetId(const std::string& filePath) const {
    return ToApi(m_impl->reg.GetId(filePath));
}

std::string AssetRegistry::GetFilePath(const AssetId& id) const {
    return m_impl->reg.GetFilePath(ToCore(id));
}

std::string AssetRegistry::GetAssetPath(const AssetId& id) const {
    return m_impl->reg.GetAssetPath(ToCore(id));
}

AssetType AssetRegistry::GetAssetType(const AssetId& id) const {
    return ToApiType(m_impl->reg.GetAssetType(ToCore(id)));
}

size_t AssetRegistry::GetAssetSize(const AssetId& id) const {
    return m_impl->reg.GetAssetSize(ToCore(id));
}

size_t AssetRegistry::GetAssetMemoryUsage() const {
    return m_impl->reg.GetAssetMemoryUsage();
}

std::shared_ptr<AnimationClip> AssetRegistry::GetAnimationClip(const AssetId& id, size_t index) const {
    if (!id.IsValid()) {
        return nullptr;
    }
    auto asset = m_impl->reg.Cache().Get<eyelib::assets::AnimationAsset>(ToCore(id));
    if (!asset || index >= asset->clips.size()) {
        return nullptr;
    }
    return AnimationClip::WrapInternal(asset->clips[index]);
}

std::shared_ptr<AnimationClip> AssetRegistry::GetAnimationClip(const std::string& path, size_t index) const {
    return GetAnimationClip(GetId(path), index);
}

std::shared_ptr<Skeleton> AssetRegistry::GetSkeleton(const AssetId& id) const {
    if (!id.IsValid()) {
        return nullptr;
    }
    auto asset = m_impl->reg.Cache().Get<eyelib::assets::AnimationAsset>(ToCore(id));
    if (!asset || !asset->skeleton) {
        return nullptr;
    }
    return Skeleton::WrapInternal(asset->skeleton);
}

std::shared_ptr<Skeleton> AssetRegistry::GetSkeleton(const std::string& path) const {
    return GetSkeleton(GetId(path));
}

std::shared_ptr<Material> AssetRegistry::GetMaterial(const AssetId& id) const {
    if (!id.IsValid()) {
        return nullptr;
    }
    auto material = m_impl->reg.Cache().Get<eyelib::render::Material>(ToCore(id));
    if (!material) {
        return nullptr;
    }
    return Material::WrapInternal(material);
}

std::shared_ptr<Material> AssetRegistry::GetMaterial(const std::string& path) const {
    auto id = GetId(path);
    if (!id.IsValid()) {
        id = LoadAsset(path, AssetType::Material);
    }
    return GetMaterial(id);
}

void AssetRegistry::AddDependency(const AssetId& asset, const AssetId& dependency) {
    m_impl->reg.AddDependency(ToCore(asset), ToCore(dependency));
}

void AssetRegistry::RemoveDependency(const AssetId& asset, const AssetId& dependency) {
    m_impl->reg.RemoveDependency(ToCore(asset), ToCore(dependency));
}

std::vector<AssetId> AssetRegistry::GetDependencies(const AssetId& id) const {
    auto deps = m_impl->reg.GetDependencies(ToCore(id));
    std::vector<AssetId> out;
    out.reserve(deps.size());
    for (const auto& d : deps) out.push_back(ToApi(d));
    return out;
}

std::vector<AssetId> AssetRegistry::GetDependents(const AssetId& id) const {
    auto deps = m_impl->reg.GetDependents(ToCore(id));
    std::vector<AssetId> out;
    out.reserve(deps.size());
    for (const auto& d : deps) out.push_back(ToApi(d));
    return out;
}

bool AssetRegistry::SaveToFile(const std::string& filepath) const {
    return m_impl->reg.SaveToFile(filepath);
}

bool AssetRegistry::LoadFromFile(const std::string& filepath) {
    return m_impl->reg.LoadFromFile(filepath);
}

void AssetRegistry::Clear() {
    m_impl->reg.Clear();
}

} // namespace api
} // namespace eyelib

