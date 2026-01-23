#include "api/assets/AssetRegistry.h"

#include "eyelib/assets/AssetRegistry.h"

namespace eyelib {
namespace api {

static eyelib::assets::AssetId ToCore(const AssetId& id) {
    return eyelib::assets::AssetId(id.high, id.low);
}

static AssetId ToApi(const eyelib::assets::AssetId& id) {
    return AssetId{ id.High(), id.Low() };
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

