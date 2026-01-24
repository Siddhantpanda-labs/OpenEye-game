#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/physics/PhysicsWorld.h"
#include <vector>

namespace eyelib {
namespace physics {

using VehicleHandle = uint32_t;

struct WheelDesc {
    math::Vec3 connectionPoint{0.0f};
    math::Vec3 direction{0.0f, -1.0f, 0.0f};
    math::Vec3 axle{-1.0f, 0.0f, 0.0f};
    float suspensionRestLength = 0.6f;
    float radius = 0.5f;
    bool isFront = false;
    float frictionSlip = 1.0f;
    float rollInfluence = 0.1f;
};

struct WheelTransform {
    math::Vec3 position{0.0f};
    math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

/**
 * @brief Vehicle physics (raycast vehicle). Uses rigid body as chassis.
 */
class Vehicle {
public:
    static VehicleHandle CreateVehicle(PhysicsWorld* world, RigidBodyHandle chassis,
                                       const std::vector<WheelDesc>& wheels);
    static void DestroyVehicle(PhysicsWorld* world, VehicleHandle handle);

    static void SetEnginePower(PhysicsWorld* world, VehicleHandle handle, float power);
    static void SetBrakeForce(PhysicsWorld* world, VehicleHandle handle, float force);
    static void SetSteeringAngle(PhysicsWorld* world, VehicleHandle handle, float angle);

    static WheelTransform GetWheelTransform(PhysicsWorld* world, VehicleHandle handle, int wheelIndex);
};

} // namespace physics
} // namespace eyelib
