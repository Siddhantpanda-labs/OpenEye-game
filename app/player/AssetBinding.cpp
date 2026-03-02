#include "AssetBinding.h"

#include "api/core/All.h"
#include "api/platform/All.h"

namespace openeye::player {

static std::string JoinPath(const std::string& base, const std::string& leaf) {
    if (base.empty()) {
        return leaf;
    }
    const char last = base.back();
    if (last == '/' || last == '\\') {
        return base + leaf;
    }
    return base + "/" + leaf;
}

static std::string ResolveAssetPath(const std::string& id, const std::vector<std::string>& roots) {
    using eyelib::api::platform::filesystem::FileExists;
    if (FileExists(id)) {
        return id;
    }
    for (const auto& root : roots) {
        const std::string candidate = JoinPath(root, id);
        if (FileExists(candidate)) {
            return candidate;
        }
    }
    return {};
}

AssetBindStats BindAssets(const eyelib::api::world::World& world,
                          const std::vector<std::string>& assetRoots,
                          eyelib::api::AssetRegistry& registry,
                          bool allowGpuAssets) {
    AssetBindStats stats{};
    const auto refs = world.CollectAssetReferences();
    for (const auto& ref : refs) {
        if (ref.id.empty()) {
            continue;
        }
        stats.referenced += 1;

        if (ref.kind == eyelib::api::world::World::AssetRefKind::Script) {
            eyelib::api::core::Log::InfoF("Script reference: %s", ref.id.c_str());
            continue;
        }

        auto type = eyelib::api::AssetType::Unknown;
        switch (ref.kind) {
            case eyelib::api::world::World::AssetRefKind::Mesh:
                type = eyelib::api::AssetType::Mesh;
                break;
            case eyelib::api::world::World::AssetRefKind::Material:
                type = eyelib::api::AssetType::Material;
                break;
            case eyelib::api::world::World::AssetRefKind::Animation:
            case eyelib::api::world::World::AssetRefKind::Skeleton:
                type = eyelib::api::AssetType::Animation;
                break;
            case eyelib::api::world::World::AssetRefKind::Script:
                type = eyelib::api::AssetType::Unknown;
                break;
        }

        const std::string resolved = ResolveAssetPath(ref.id, assetRoots);
        if (resolved.empty()) {
            eyelib::api::core::Log::WarnF("Missing asset: %s", ref.id.c_str());
            stats.missing += 1;
            continue;
        }

        if (type == eyelib::api::AssetType::Unknown) {
            eyelib::api::core::Log::WarnF("Unknown asset type for: %s", resolved.c_str());
            stats.unknownType += 1;
            continue;
        }

        if (!allowGpuAssets
            && (type == eyelib::api::AssetType::Mesh
                || type == eyelib::api::AssetType::Material
                || type == eyelib::api::AssetType::Shader
                || type == eyelib::api::AssetType::Texture)) {
            eyelib::api::core::Log::InfoF("Skipping GPU asset load (headless): %s", resolved.c_str());
            continue;
        }

        try {
            const bool alreadyRegistered = registry.IsRegistered(resolved);
            registry.LoadAsset(resolved, type);
            if (!alreadyRegistered) {
                eyelib::api::core::Log::InfoF("Loaded asset: %s", resolved.c_str());
            }
            stats.loaded += 1;
        } catch (const std::exception& e) {
            eyelib::api::core::Log::ErrorF("Failed to load asset: %s - %s", resolved.c_str(), e.what());
            stats.missing += 1;
        } catch (...) {
            eyelib::api::core::Log::ErrorF("Failed to load asset: %s (unknown exception)", resolved.c_str());
            stats.missing += 1;
        }
    }

    return stats;
}

} // namespace openeye::player
