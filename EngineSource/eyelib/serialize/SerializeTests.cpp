#include "SerializeTests.h"

#include "eyelib/core/Log.h"
#include "eyelib/scene/Scene.h"
#include "eyelib/serialize/BinarySerializer.h"

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
} // namespace

bool SerializeTests::Run() {
    bool ok = true;
    ok = TestSceneRoundTrip() && ok;
    if (ok) {
        core::Log::Info("SerializeTests: all tests passed.");
    }
    return ok;
}

} // namespace serialize
} // namespace eyelib
