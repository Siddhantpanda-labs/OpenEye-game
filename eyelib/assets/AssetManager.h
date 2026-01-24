#pragma once

#include "eyelib/assets/AssetRegistry.h"

namespace eyelib {
namespace assets {

class AssetManager {
public:
    static AssetManager& Get();

    AssetRegistry& Registry() { return m_registry; }
    AssetCache& Cache() { return m_registry.Cache(); }

private:
    AssetManager() = default;
    AssetRegistry m_registry;
};

} // namespace assets
} // namespace eyelib
