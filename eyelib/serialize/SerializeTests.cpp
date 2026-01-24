#include "SerializeTests.h"

#include "eyelib/core/Log.h"
#include "eyelib/scene/Scene.h"
#include "eyelib/serialize/BinarySerializer.h"
#include "eyelib/serialize/WorldSnapshot.h"
#include "eyelib/ecs/components/Transform.h"
#include "eyelib/ecs/components/Name.h"
#include "entt/entt.hpp"

namespace eyelib {
namespace serialize {

namespace {
static bool TestSceneRoundTrip() {
    scene::Scene scene;
    auto parent = scene.CreateEntity("Parent");
    auto child = scene.CreateEntity("Child");
    scene.SetParent(child, parent);

    const auto a = BinarySerializer::SerializeScene(scene, BinaryCompression::None);
    const auto b = BinarySerializer::SerializeScene(scene, BinaryCompression::None);
    if (a != b) {
        core::Log::Error("SerializeTests: scene deterministic serialization failed.");
        return false;
    }

    scene::Scene scene2;
    if (!BinarySerializer::DeserializeScene(scene2, a)) {
        core::Log::Error("SerializeTests: scene deserialize failed.");
        return false;
    }

    const auto entities = scene2.GetEntities();
    if (entities.size() != 2) {
        core::Log::Error("SerializeTests: scene entity count mismatch.");
        return false;
    }

    auto child2 = scene2.FindEntity("Child");
    auto parent2 = scene2.FindEntity("Parent");
    if (child2 == scene::NullEntity || parent2 == scene::NullEntity) {
        core::Log::Error("SerializeTests: scene name lookup failed.");
        return false;
    }
    if (scene2.GetParent(child2) != parent2) {
        core::Log::Error("SerializeTests: scene parent mapping mismatch.");
        return false;
    }

    return true;
}

static bool TestWorldSnapshot() {
    ecs::World world;
    auto a = world.CreateEntity("A");
    auto b = world.CreateEntity("B");

    if (auto* tr = world.TryGetComponent<ecs::components::Transform>(b)) {
        tr->parent = a;
        tr->dirty = true;
    }

    const auto snapshot = WorldSnapshot::Capture(world);
    ecs::World restored;
    if (!WorldSnapshot::Restore(restored, snapshot)) {
        core::Log::Error("SerializeTests: world snapshot restore failed.");
        return false;
    }

    auto view = restored.Registry().view<ecs::components::Name>();
    ecs::Entity restoredA = ecs::NullEntity;
    ecs::Entity restoredB = ecs::NullEntity;
    for (auto entity : view) {
        const auto& name = view.get<ecs::components::Name>(entity).name;
        if (name == "A") restoredA = static_cast<ecs::Entity>(static_cast<entt::id_type>(entity));
        if (name == "B") restoredB = static_cast<ecs::Entity>(static_cast<entt::id_type>(entity));
    }
    if (restoredA == ecs::NullEntity || restoredB == ecs::NullEntity) {
        core::Log::Error("SerializeTests: world names missing.");
        return false;
    }
    auto trB = restored.TryGetComponent<ecs::components::Transform>(restoredB);
    if (!trB || trB->parent == ecs::NullEntity) {
        core::Log::Error("SerializeTests: world parent restore failed.");
        return false;
    }
    if (trB->parent != restoredA) {
        core::Log::Error("SerializeTests: world parent mapping mismatch.");
        return false;
    }
    return true;
}
} // namespace

bool SerializeTests::Run() {
    bool ok = true;
    ok = TestSceneRoundTrip() && ok;
    ok = TestWorldSnapshot() && ok;
    if (ok) {
        core::Log::Info("SerializeTests: all tests passed.");
    }
    return ok;
}

} // namespace serialize
} // namespace eyelib
