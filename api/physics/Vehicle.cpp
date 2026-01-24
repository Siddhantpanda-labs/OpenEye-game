#include "api/physics/Vehicle.h"

namespace eyelib {
namespace api {

VehicleHandle Vehicle::CreateVehicle(PhysicsWorld& world, RigidBodyHandle chassis,
                                     const std::vector<WheelDesc>& wheels) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::Vehicle::CreateVehicle(w, chassis, wheels) : 0;
}

void Vehicle::DestroyVehicle(PhysicsWorld& world, VehicleHandle handle) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::Vehicle::DestroyVehicle(w, handle);
}

void Vehicle::SetEnginePower(PhysicsWorld& world, VehicleHandle handle, float power) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::Vehicle::SetEnginePower(w, handle, power);
}

void Vehicle::SetBrakeForce(PhysicsWorld& world, VehicleHandle handle, float force) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::Vehicle::SetBrakeForce(w, handle, force);
}

void Vehicle::SetSteeringAngle(PhysicsWorld& world, VehicleHandle handle, float angle) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::Vehicle::SetSteeringAngle(w, handle, angle);
}

WheelTransform Vehicle::GetWheelTransform(PhysicsWorld& world, VehicleHandle handle, int wheelIndex) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::Vehicle::GetWheelTransform(w, handle, wheelIndex) : WheelTransform{};
}

} // namespace api
} // namespace eyelib
