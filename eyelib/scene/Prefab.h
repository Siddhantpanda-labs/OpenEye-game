#pragma once

#include "eyelib/ecs/ComponentRegistry.h"
#include "eyelib/scene/Scene.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace scene {

using PrefabHandle = uint32_t;
static constexpr PrefabHandle NullPrefab = 0;

struct PrefabComponent {
    ecs::ComponentTypeId type = 0;
    std::shared_ptr<void> data;
};

struct PrefabNode {
    std::string name;
    math::Transform transform{};
    std::vector<PrefabComponent> components;
    std::vector<PrefabNode> children;
};

class Prefab {
public:
    struct PrefabData {
        std::string name;
        PrefabNode root;
    };

    static PrefabHandle CreatePrefab(const std::string& name);
    static PrefabHandle CreateFromEntity(Scene& scene, Entity entity, const std::string& name);
    static PrefabHandle CreateFromHierarchy(Scene& scene, Entity root, const std::string& name);
    static bool IsValid(PrefabHandle handle);
    static void DestroyPrefab(PrefabHandle handle);

    static const std::string& GetName(PrefabHandle handle);
    static void SetName(PrefabHandle handle, const std::string& name);

    static void AddComponent(PrefabHandle handle, ecs::ComponentTypeId type, const void* data);
    static void* GetComponent(PrefabHandle handle, ecs::ComponentTypeId type);

    static bool SavePrefab(PrefabHandle handle, const std::string& path);
    static PrefabHandle LoadPrefab(const std::string& path);

    static const PrefabNode* GetRoot(PrefabHandle handle);
    static ecs::ComponentRegistry& GetRegistry();

private:
    static ecs::ComponentRegistry& Registry();
    static PrefabData* GetDataMutable(PrefabHandle handle);
    static const PrefabData* GetData(PrefabHandle handle);

    static PrefabNode CaptureNode(Scene& scene, Entity entity);
    static void CaptureComponents(Scene& scene, Entity entity, PrefabNode& node);
};

} // namespace scene
} // namespace eyelib
