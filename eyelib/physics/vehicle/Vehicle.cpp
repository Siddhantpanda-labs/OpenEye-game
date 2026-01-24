#include "eyelib/physics/vehicle/Vehicle.h"
#include "eyelib/core/Log.h"

#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/Vehicle/btRaycastVehicle.h>
#include <BulletDynamics/Vehicle/btVehicleRaycaster.h>
#include <BulletDynamics/Vehicle/btWheelInfo.h>
#include <unordered_map>

namespace eyelib {
namespace physics {

namespace {
btVector3 ToBt(const math::Vec3& v) { return btVector3(v.x, v.y, v.z); }
math::Vec3 ToGlm(const btVector3& v) { return math::Vec3(v.x(), v.y(), v.z()); }
math::Quat ToGlm(const btQuaternion& q) { return math::Quat(q.w(), q.x(), q.y(), q.z()); }

struct VehicleEntry {
    btRaycastVehicle* vehicle = nullptr;
    btVehicleRaycaster* raycaster = nullptr;
    btRigidBody* chassis = nullptr;
    std::vector<WheelDesc> wheels;
};

static VehicleHandle s_nextHandle = 1;
static std::unordered_map<VehicleHandle, VehicleEntry> s_vehicles;
} // namespace

VehicleHandle Vehicle::CreateVehicle(PhysicsWorld* world, RigidBodyHandle chassis,
                                    const std::vector<WheelDesc>& wheels) {
    if (!world || wheels.empty()) return 0;
    btDynamicsWorld* w = world->GetWorldInternal();
    btRigidBody* body = world->GetRigidBodyInternal(chassis);
    if (!w || !body) return 0;

    btRaycastVehicle::btVehicleTuning tuning;
    auto* raycaster = new btDefaultVehicleRaycaster(w);
    auto* vehicle = new btRaycastVehicle(tuning, body, raycaster);
    vehicle->setCoordinateSystem(0, 1, 2); // X: right, Y: up, Z: forward

    for (const auto& wheel : wheels) {
        btWheelInfo& info = vehicle->addWheel(
            ToBt(wheel.connectionPoint),
            ToBt(wheel.direction),
            ToBt(wheel.axle),
            wheel.suspensionRestLength,
            wheel.radius,
            tuning,
            wheel.isFront
        );
        info.m_frictionSlip = wheel.frictionSlip;
        info.m_rollInfluence = wheel.rollInfluence;
    }

    w->addVehicle(vehicle);

    VehicleHandle h = s_nextHandle++;
    VehicleEntry e;
    e.vehicle = vehicle;
    e.raycaster = raycaster;
    e.chassis = body;
    e.wheels = wheels;
    s_vehicles.emplace(h, std::move(e));
    return h;
}

void Vehicle::DestroyVehicle(PhysicsWorld* world, VehicleHandle handle) {
    if (!world) return;
    auto it = s_vehicles.find(handle);
    if (it == s_vehicles.end()) return;
    btDynamicsWorld* w = world->GetWorldInternal();
    if (w && it->second.vehicle) w->removeVehicle(it->second.vehicle);
    delete it->second.vehicle;
    delete it->second.raycaster;
    s_vehicles.erase(it);
}

void Vehicle::SetEnginePower(PhysicsWorld* world, VehicleHandle handle, float power) {
    if (!world) return;
    auto it = s_vehicles.find(handle);
    if (it == s_vehicles.end() || !it->second.vehicle) return;
    auto* v = it->second.vehicle;
    for (int i = 0; i < v->getNumWheels(); ++i) {
        v->applyEngineForce(power, i);
    }
}

void Vehicle::SetBrakeForce(PhysicsWorld* world, VehicleHandle handle, float force) {
    if (!world) return;
    auto it = s_vehicles.find(handle);
    if (it == s_vehicles.end() || !it->second.vehicle) return;
    auto* v = it->second.vehicle;
    for (int i = 0; i < v->getNumWheels(); ++i) {
        v->setBrake(force, i);
    }
}

void Vehicle::SetSteeringAngle(PhysicsWorld* world, VehicleHandle handle, float angle) {
    if (!world) return;
    auto it = s_vehicles.find(handle);
    if (it == s_vehicles.end() || !it->second.vehicle) return;
    auto* v = it->second.vehicle;
    for (int i = 0; i < v->getNumWheels(); ++i) {
        bool isFront = (i < static_cast<int>(it->second.wheels.size())) ? it->second.wheels[i].isFront : true;
        if (isFront) v->setSteeringValue(angle, i);
    }
}

WheelTransform Vehicle::GetWheelTransform(PhysicsWorld* world, VehicleHandle handle, int wheelIndex) {
    WheelTransform t{};
    if (!world) return t;
    auto it = s_vehicles.find(handle);
    if (it == s_vehicles.end() || !it->second.vehicle) return t;
    auto* v = it->second.vehicle;
    if (wheelIndex < 0 || wheelIndex >= v->getNumWheels()) return t;
    v->updateWheelTransform(wheelIndex, true);
    const btTransform& wt = v->getWheelTransformWS(wheelIndex);
    t.position = ToGlm(wt.getOrigin());
    t.rotation = ToGlm(wt.getRotation());
    return t;
}

} // namespace physics
} // namespace eyelib
