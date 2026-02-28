#pragma once

#include "api/physics/PhysicsWorld.h"
#include "eyelib/math/Types.h"

#include <vector>

namespace eyelib {
namespace api {
namespace physics {

using VehicleHandle = uint32_t;

struct WheelDesc {
    eyelib::math::Vec3 connectionPoint{0.0f};
    eyelib::math::Vec3 direction{0.0f, -1.0f, 0.0f};
    eyelib::math::Vec3 axle{-1.0f, 0.0f, 0.0f};
    float suspensionRestLength = 0.6f;
    float radius = 0.5f;
    bool isFront = false;
    float frictionSlip = 1.0f;
    float rollInfluence = 0.1f;
};

struct WheelTransform {
    eyelib::math::Vec3 position{0.0f};
    eyelib::math::Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

class Vehicle {
public:
    static VehicleHandle CreateVehicle(PhysicsWorld& world, RigidBodyHandle chassis,
                                       const std::vector<WheelDesc>& wheels);
    static void DestroyVehicle(PhysicsWorld& world, VehicleHandle handle);
    static void SetEnginePower(PhysicsWorld& world, VehicleHandle handle, float power);
    static void SetBrakeForce(PhysicsWorld& world, VehicleHandle handle, float force);
    static void SetSteeringAngle(PhysicsWorld& world, VehicleHandle handle, float angle);
    static WheelTransform GetWheelTransform(PhysicsWorld& world, VehicleHandle handle, int wheelIndex);
};

} // namespace physics
} // namespace api
} // namespace eyelib
