#pragma once

#include "Prefab.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace scene {

using PrefabVariantHandle = uint32_t;
static constexpr PrefabVariantHandle NullPrefabVariant = 0;

class PrefabVariant {
public:
    struct VariantData {
        PrefabHandle base = NullPrefab;
        std::vector<PrefabComponent> overrides;
    };

    static PrefabVariantHandle CreateVariant(PrefabHandle basePrefab);
    static bool IsValid(PrefabVariantHandle handle);
    static PrefabHandle GetBasePrefab(PrefabVariantHandle handle);

    static void OverrideComponent(PrefabVariantHandle handle, ecs::ComponentTypeId type, const void* data);
    static void* GetOverride(PrefabVariantHandle handle, ecs::ComponentTypeId type);

private:
    static ecs::ComponentRegistry& Registry();
    static VariantData* GetData(PrefabVariantHandle handle);
};

} // namespace scene
} // namespace eyelib
