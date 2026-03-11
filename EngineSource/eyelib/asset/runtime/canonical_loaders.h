#pragma once

/**
 * @file canonical_loaders.h
 * @brief Minimal facade for loading canonical-format assets by GUID.
 * Implementation: asset::AssetManager. Set database and asset root before use.
 */

#include "asset/core/guid.h"
#include "asset/core/import_mesh.h"
#include "asset/core/import_animation.h"
#include "asset/readers/material_reader.h"
#include "asset/readers/texture_reader.h"
#include "asset/runtime/asset_manager.h"

#include <memory>

namespace asset {

/** Load canonical mesh (.omesh / .oea). Returns cached or newly loaded; null if missing or failed. */
inline std::shared_ptr<ImportMesh> LoadMesh(const Guid& guid) {
    return AssetManager::Get().LoadMesh(guid);
}

/** Load canonical material (.mat). Returns cached or newly loaded; null if missing or failed. */
inline std::shared_ptr<LoadedMaterial> LoadMaterial(const Guid& guid) {
    return AssetManager::Get().LoadMaterial(guid);
}

/** Load canonical texture (.tex or image). Returns cached or newly loaded; null if missing or failed. */
inline std::shared_ptr<LoadedTexture> LoadTexture(const Guid& guid) {
    return AssetManager::Get().LoadTexture(guid);
}

/** Load canonical animation (.anim). Returns cached or newly loaded; null if missing or failed. */
inline std::shared_ptr<ImportAnimation> LoadAnimation(const Guid& guid) {
    return AssetManager::Get().LoadAnimation(guid);
}

} // namespace asset
