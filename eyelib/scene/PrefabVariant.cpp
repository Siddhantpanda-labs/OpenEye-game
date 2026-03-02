#include "PrefabVariant.h"

#include "eyelib/core/Log.h"

namespace eyelib {
namespace scene {

namespace {
struct VariantStore {
    std::unordered_map<PrefabVariantHandle, PrefabVariant::VariantData> variants;
    PrefabVariantHandle nextHandle = 1;
};

VariantStore& Store() {
    static VariantStore store;
    return store;
}
} // namespace

world::ComponentRegistry& PrefabVariant::Registry() {
    return Prefab::GetRegistry();
}

PrefabVariant::VariantData* PrefabVariant::GetData(PrefabVariantHandle handle) {
    auto& map = Store().variants;
    auto it = map.find(handle);
    return it != map.end() ? &it->second : nullptr;
}

PrefabVariantHandle PrefabVariant::CreateVariant(PrefabHandle basePrefab) {
    if (!Prefab::IsValid(basePrefab)) return NullPrefabVariant;
    auto& store = Store();
    const PrefabVariantHandle handle = store.nextHandle++;
    VariantData data;
    data.base = basePrefab;
    store.variants.emplace(handle, std::move(data));
    return handle;
}

bool PrefabVariant::IsValid(PrefabVariantHandle handle) {
    return GetData(handle) != nullptr;
}

PrefabHandle PrefabVariant::GetBasePrefab(PrefabVariantHandle handle) {
    auto* data = GetData(handle);
    return data ? data->base : NullPrefab;
}

void PrefabVariant::OverrideComponent(PrefabVariantHandle handle, world::ComponentTypeId type, const void* data) {
    auto* variant = GetData(handle);
    if (!variant || !data) return;
    const world::ComponentTypeInfo* info = Registry().GetTypeInfo(type);
    if (!info || !info->clone || !info->destroy) return;

    PrefabComponent comp;
    comp.type = type;
    comp.data = std::shared_ptr<void>(info->clone(data), info->destroy);
    variant->overrides.push_back(std::move(comp));
}

void* PrefabVariant::GetOverride(PrefabVariantHandle handle, world::ComponentTypeId type) {
    auto* variant = GetData(handle);
    if (!variant) return nullptr;
    for (auto& comp : variant->overrides) {
        if (comp.type == type) return comp.data.get();
    }
    return nullptr;
}

} // namespace scene
} // namespace eyelib
