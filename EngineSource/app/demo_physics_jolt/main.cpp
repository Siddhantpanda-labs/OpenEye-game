/**
 * Jolt physics demo: floor + falling sphere, step simulation, raycast, AABB query.
 * Run from build/bin/Debug or Release.
 */

#include "api/physics/PhysicsWorld.h"
#include "api/physics/Query.h"
#include "eyelib/math/AABB.h"
#include "eyelib/math/Types.h"

#include <iostream>
#include <cstdlib>

using namespace eyelib::api::physics;

int main() {
    std::cout << "Jolt Physics Demo\n";
    std::cout << "-----------------\n";

    std::cout << "Creating PhysicsWorld...\n";
    PhysicsWorld world;
    std::cout << "Initializing (gravity 0,-9.81,0)...\n";
    if (!world.Initialize(eyelib::math::Vec3(0.0f, -9.81f, 0.0f))) {
        std::cerr << "Failed to initialize physics world.\n";
        return 1;
    }

    std::cout << "Creating floor (static box)...\n";
    // Contact callback (optional)
    world.SetContactCallback([](RigidBodyHandle a, RigidBodyHandle b,
                              const eyelib::math::Vec3& pos, const eyelib::math::Vec3& normal) {
        std::cout << "  Contact: bodies " << a << " <-> " << b
                  << " at (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
    });

    // Floor: static box (mass 0)
    CollisionShapeHandle floorShape = world.CreateBoxShape(eyelib::math::Vec3(100.0f, 1.0f, 100.0f));
    RigidBodyHandle floorBody = world.CreateRigidBody(
        eyelib::math::Vec3(0.0f, -1.0f, 0.0f),
        eyelib::math::Quat(1.0f, 0.0f, 0.0f, 0.0f),
        floorShape,
        0.0f  // static
    );
    if (!floorBody) {
        std::cerr << "Failed to create floor.\n";
        return 1;
    }
    std::cout << "Floor body handle: " << floorBody << "\n";

    std::cout << "Creating sphere (dynamic, 1m above floor)...\n";
    // Sphere: dynamic, 1 m above floor
    CollisionShapeHandle sphereShape = world.CreateSphereShape(0.5f);
    RigidBodyHandle sphereBody = world.CreateRigidBody(
        eyelib::math::Vec3(0.0f, 2.0f, 0.0f),
        eyelib::math::Quat(1.0f, 0.0f, 0.0f, 0.0f),
        sphereShape,
        1.0f
    );
    if (!sphereBody) {
        std::cerr << "Failed to create sphere.\n";
        return 1;
    }

    const float dt = 1.0f / 60.0f;
    const int steps = 120;  // 2 seconds

    std::cout << "Stepping " << steps << " frames (dt=" << dt << "s)...\n";
    for (int i = 0; i < steps; ++i) {
        world.Step(dt, 3, 1.0f / 60.0f);
        if (i % 30 == 0 || i == steps - 1) {
            auto t = world.GetRigidBodyTransform(sphereBody);
            if (t)
                std::cout << "  step " << i << ": sphere at (" << t->position.x << ", "
                          << t->position.y << ", " << t->position.z << ")\n";
        }
    }

    std::cout << "Body count: " << world.GetBodyCount() << "\n";

    // Raycast: from above downward
    RaycastResult ray = Query::Raycast(world,
        eyelib::math::Vec3(0.0f, 5.0f, 0.0f),
        eyelib::math::Vec3(0.0f, -5.0f, 0.0f));
    std::cout << "Raycast (0,5,0) -> (0,-5,0): hit=" << (ray.hit ? "yes" : "no");
    if (ray.hit)
        std::cout << " body=" << ray.body << " point=(" << ray.point.x << "," << ray.point.y << "," << ray.point.z << ") distance=" << ray.distance;
    std::cout << "\n";

    // AABB query: box around origin
    eyelib::math::AABB box(eyelib::math::Vec3(-2.0f, -2.0f, -2.0f), eyelib::math::Vec3(2.0f, 2.0f, 2.0f));
    std::vector<RigidBodyHandle> inBox = Query::QueryAABB(world, box);
    std::cout << "QueryAABB([-2,-2,-2]..[2,2,2]): " << inBox.size() << " body(ies)\n";

    world.Shutdown();
    std::cout << "Done.\n";
    return 0;
}
