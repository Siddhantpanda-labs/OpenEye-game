#include "ModelLoader.h"
#include "api/asset/AssetRegistry.h"
#include "api/asset/AssetTypes.h"
#include "api/asset/IGPUResourceResolver.h"
#include "api/asset/ModelAsset.h"
#include "api/render/Mesh.h"
#include "api/render/Material.h"
#include "eyelib/math/Types.h"


namespace eyelib {
namespace api {

namespace {

/** Make path relative to asset root when possible so registry/database lookup works. */
std::string ToRelativePath(const std::string& path, const std::string& assetRoot) {
    if (assetRoot.empty() || path.size() <= assetRoot.size()) return path;
    if (path.compare(0, assetRoot.size(), assetRoot) != 0) return path;
    std::string rel = path.substr(assetRoot.size());
    while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) rel.erase(0, 1);
    return rel;
}

math::Mat4 IdentityMatrix() {
    return math::Mat4{1.0f};
}

} // namespace

std::shared_ptr<ModelAsset> ModelLoader::LoadModel(AssetRegistry* registry, const std::string& path) {
    if (!registry || path.empty()) return nullptr;

    const std::string assetRoot = registry->GetAssetRoot();
    const std::string relPath = ToRelativePath(path, assetRoot);

    const AssetId meshId = registry->LoadAsset(relPath, AssetType::Mesh);
    if (!meshId.IsValid()) return nullptr;

    IGPUResourceResolver* resolver = registry->GetGPUResolver();
    if (!resolver) return nullptr;

    std::shared_ptr<::eyelib::render::Mesh> renderMesh = resolver->GetMesh(meshId);
    if (!renderMesh || !renderMesh->IsValid()) return nullptr;

    std::shared_ptr<Mesh> apiMesh = Mesh::WrapInternal(renderMesh);
    if (!apiMesh) return nullptr;

    std::vector<std::shared_ptr<Material>> materials;
    const std::vector<AssetId> deps = registry->GetDependencies(meshId);
    for (const AssetId& depId : deps) {
        if (registry->GetAssetType(depId) != AssetType::Material) continue;
        std::shared_ptr<Material> mat = registry->GetMaterial(depId);
        if (mat) materials.push_back(std::move(mat));
    }

    std::vector<std::shared_ptr<Mesh>> meshes = { std::move(apiMesh) };
    std::vector<math::Mat4> transforms = { IdentityMatrix() };

    return ModelAsset::WrapInternal(
        std::move(meshes),
        std::move(materials),
        std::move(transforms),
        nullptr,
        nullptr);
}

std::shared_ptr<ModelAsset> ModelLoader::LoadModel(const std::string& path) {
    (void)path;
    return nullptr;
}

} // namespace api
} // namespace eyelib
