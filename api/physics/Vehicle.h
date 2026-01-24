#pragma once

#include "api/physics/PhysicsWorld.h"
#include "eyelib/physics/vehicle/Vehicle.h"
#include <vector>

namespace eyelib {
namespace api {

using VehicleHandle = eyelib::physics::VehicleHandle;
using WheelDesc = eyelib::physics::WheelDesc;
using WheelTransform = eyelib::physics::WheelTransform;

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

} // namespace api
} // namespace eyelib
