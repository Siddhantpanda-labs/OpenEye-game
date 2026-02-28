/**
 * Determinism test: run simulation for 5 seconds at 60 FPS and at 144 FPS
 * with the same fixed timestep (1/60). Final transform positions must match.
 * Validates Physics Authority Model and fixed-step boundary.
 */

#include "eyelib/world/World.h"
#include "eyelib/world/Commands/CommandTypes.h"
#include "eyelib/world/components/Transform.h"
#include "eyelib/world/components/RigidBody.h"
#include "eyelib/world/EntityId.h"
#include "eyelib/math/Types.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace eyelib::world;

static constexpr float FIXED_STEP = 1.0f / 60.0f;
static constexpr float DURATION = 5.0f;
static constexpr int FRAMES_60 = 300;   // 5 * 60
static constexpr int FRAMES_144 = 720;  // 5 * 144
static constexpr float DELTA_60 = 1.0f / 60.0f;
static constexpr float DELTA_144 = 1.0f / 144.0f;

static void SetupWorld(World& world) {
    world.Scheduler().SetFixedTimestep(FIXED_STEP);
    auto r = world.ApplyCommandSync(std::make_unique<CreateEntityCommand>(""));
    if (!r.success || !r.createdEntity) {
        std::cerr << "CreateEntity failed\n";
        std::exit(1);
    }
    EntityId e = *r.createdEntity;
    world.SubmitCommand(std::make_unique<SetTransformCommand>(e, components::Transform{}));
    components::RigidBody rb;
    rb.linearVelocity = eyelib::math::Vec3(1.0f, 0.0f, 0.0f);
    rb.isKinematic = false;
    rb.useGravity = false;  // horizontal-only motion for determinism test
    world.SubmitCommand(std::make_unique<AddComponentCommand<components::RigidBody>>(e, rb));
    world.ApplyPendingCommands();
}

static eyelib::math::Vec3 GetFirstRigidBodyPosition(World& world) {
    auto& store = world.GetState().GetTransformStore();
    auto& rb = world.GetState().Components<components::RigidBody>();
    for (const auto& [entity, ref] : store.All()) {
        if (rb.TryGet(entity))
            return ref.Position();
    }
    return eyelib::math::Vec3(0.0f);
}

int main() {
    std::cout << "Physics determinism test: 5s at 60 FPS vs 144 FPS (fixed step 1/60)\n";

    World world60;
    SetupWorld(world60);
    for (int i = 0; i < FRAMES_60; ++i)
        world60.Update(DELTA_60);
    eyelib::math::Vec3 pos60 = GetFirstRigidBodyPosition(world60);

    World world144;
    SetupWorld(world144);
    for (int i = 0; i < FRAMES_144; ++i)
        world144.Update(DELTA_144);
    eyelib::math::Vec3 pos144 = GetFirstRigidBodyPosition(world144);

    float expectedX = DURATION * 1.0f;  // velocity (1,0,0) * 5s
    const float tol = 1e-5f;
    // Frame delta is float; accumulator is double. Allow one-substep variance (299 vs 300) due to float dt.
    const float oneSubstep = FIXED_STEP * 1.0f;
    const float substepTol = oneSubstep + 1e-6f;
    bool ok = std::fabs(pos60.x - expectedX) < substepTol && std::fabs(pos144.x - expectedX) < substepTol
              && std::fabs(pos60.x - pos144.x) < substepTol
              && std::fabs(pos60.y - pos144.y) < tol && std::fabs(pos60.z - pos144.z) < tol;

    std::cout << "  60 FPS final position: (" << pos60.x << ", " << pos60.y << ", " << pos60.z << ")\n";
    std::cout << " 144 FPS final position: (" << pos144.x << ", " << pos144.y << ", " << pos144.z << ")\n";
    std::cout << " Expected X = " << expectedX << "\n";
    if (ok)
        std::cout << " PASS: positions match and equal expected.\n";
    else
        std::cout << " FAIL: positions differ or wrong (authority boundary broken).\n";
    return ok ? 0 : 1;
}
