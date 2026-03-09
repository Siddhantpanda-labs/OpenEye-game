/**
 * Benchmark: TransformSystem::Update with 100k entities in a chain hierarchy.
 * Build: OpenEye with OPENEYE_BUILD_TESTS or default.
 * Run from build/bin/Debug or build/bin/Release.
 *
 * Reports:
 *   - Setup time (create entities + transforms + hierarchy)
 *   - First Update (includes linearized order rebuild)
 *   - Steady-state Update (dirty propagation, no structural change)
 *   - Full-recompute style: MarkSubtreeDirty(root) then Update
 */

#include "eyelib/world/EntityRegistry.h"
#include "eyelib/world/WorldState.h"
#include "eyelib/world/systems/TransformSystem.h"
#include "eyelib/math/Transform.h"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <vector>

using namespace eyelib::world;

static double NowSeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

int main() {
    constexpr size_t N = 10000u;  // Use 100000u for 100k-entity scenario (Release recommended)
    std::cout << "Transform benchmark: " << N << " entities (chain hierarchy)\n";

    WorldState state;
    auto& store = state.GetTransformStore();
    auto& hierarchy = state.GraphHierarchy();
    std::vector<EntityId> entities;
    entities.reserve(N);

    double t0 = NowSeconds();
    for (size_t i = 0; i < N; ++i) {
        EntityId e = state.Entities().Create();
        entities.push_back(e);
        store.Add(e, eyelib::math::Transform{});
        if (i > 0)
            hierarchy.SetParent(e, entities[i - 1]);
    }
    double setupTime = NowSeconds() - t0;
    std::cout << "  Setup: " << (setupTime * 1000.0) << " ms\n";

    systems::TransformSystem transformSystem;

    t0 = NowSeconds();
    transformSystem.Update(state);
    double firstUpdate = NowSeconds() - t0;
    std::cout << "  First Update (rebuild linearized order): " << (firstUpdate * 1000.0) << " ms\n";

    const int warmup = 5;
    for (int i = 0; i < warmup; ++i)
        transformSystem.Update(state);

    const int runs = 20;
    t0 = NowSeconds();
    for (int i = 0; i < runs; ++i)
        transformSystem.Update(state);
    double steadyTime = (NowSeconds() - t0) / runs;
    std::cout << "  Steady-state Update (no changes, " << runs << " runs avg): " << (steadyTime * 1000.0) << " ms\n";

    store.MarkSubtreeDirty(hierarchy, entities[0]);
    t0 = NowSeconds();
    transformSystem.Update(state);
    double fullRecompute = NowSeconds() - t0;
    std::cout << "  Full subtree recompute (MarkSubtreeDirty root + Update): " << (fullRecompute * 1000.0) << " ms\n";

    store.MarkLocalDirty(entities[N - 1]);
    t0 = NowSeconds();
    transformSystem.Update(state);
    double singleLeaf = NowSeconds() - t0;
    std::cout << "  Single leaf dirty (MarkLocalDirty last + Update): " << (singleLeaf * 1000.0) << " ms\n";

    std::cout << "Done.\n";
    return 0;
}
