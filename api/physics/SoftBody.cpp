#include "api/physics/SoftBody.h"

namespace eyelib {
namespace api {

SoftBodyHandle SoftBody::CreateCloth(PhysicsWorld& world, const eyelib::math::Vec3& position,
                                     int widthSegments, int heightSegments,
                                     float width, float height, float mass) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::SoftBodyFactory::CreateCloth(w, position, widthSegments, heightSegments, width, height, mass) : 0;
}

SoftBodyHandle SoftBody::CreateRope(PhysicsWorld& world, const eyelib::math::Vec3& startPos,
                                    const eyelib::math::Vec3& endPos, int segments, float mass) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::SoftBodyFactory::CreateRope(w, startPos, endPos, segments, mass) : 0;
}

SoftBodyHandle SoftBody::CreateVolumeFromMesh(PhysicsWorld& world,
                                              const std::vector<eyelib::math::Vec3>& vertices,
                                              const std::vector<int>& indices,
                                              const eyelib::math::Vec3& position,
                                              float mass) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::SoftBodyFactory::CreateVolumeFromMesh(w, vertices, indices, position, mass) : 0;
}

SoftBodyHandle SoftBody::CreateVolumeSphere(PhysicsWorld& world, const eyelib::math::Vec3& position,
                                            float radius, int resolution, float mass, float pressure) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::SoftBodyFactory::CreateVolumeSphere(w, position, radius, resolution, mass, pressure) : 0;
}

void SoftBody::SetMass(PhysicsWorld& world, SoftBodyHandle body, float mass) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::SoftBodyFactory::SetMass(w, body, mass);
}

void SoftBody::SetStiffness(PhysicsWorld& world, SoftBodyHandle body, float stiffness) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::SoftBodyFactory::SetStiffness(w, body, stiffness);
}

void SoftBody::SetDamping(PhysicsWorld& world, SoftBodyHandle body, float damping) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::SoftBodyFactory::SetDamping(w, body, damping);
}

void SoftBody::AttachToRigidBody(PhysicsWorld& world, SoftBodyHandle body, RigidBodyHandle rigidBody,
                                 int nodeIndex, bool disableCollision, float influence) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::SoftBodyFactory::AttachToRigidBody(w, body, rigidBody, nodeIndex, disableCollision, influence);
}

eyelib::math::Vec3 SoftBody::GetNodePosition(PhysicsWorld& world, SoftBodyHandle body, int nodeIndex) {
    auto* w = world.GetInternal();
    return w ? eyelib::physics::SoftBodyFactory::GetNodePosition(w, body, nodeIndex) : eyelib::math::Vec3(0.f);
}

void SoftBody::SetNodePosition(PhysicsWorld& world, SoftBodyHandle body, int nodeIndex, const eyelib::math::Vec3& position) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::SoftBodyFactory::SetNodePosition(w, body, nodeIndex, position);
}

void SoftBody::ApplyForce(PhysicsWorld& world, SoftBodyHandle body, int nodeIndex, const eyelib::math::Vec3& force) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::SoftBodyFactory::ApplyForce(w, body, nodeIndex, force);
}

void SoftBody::Destroy(PhysicsWorld& world, SoftBodyHandle body) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::SoftBodyFactory::Destroy(w, body);
}

void SoftBody::ClothSetWind(PhysicsWorld& world, SoftBodyHandle cloth,
                            const eyelib::math::Vec3& direction, float strength) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::Cloth::SetWind(w, cloth, direction, strength);
}

void SoftBody::ClothSetCollision(PhysicsWorld& world, SoftBodyHandle cloth, bool enabled) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::Cloth::SetCollision(w, cloth, enabled);
}

void SoftBody::ClothSetSelfCollision(PhysicsWorld& world, SoftBodyHandle cloth, bool enabled) {
    auto* w = world.GetInternal();
    if (w) eyelib::physics::Cloth::SetSelfCollision(w, cloth, enabled);
}

} // namespace api
} // namespace eyelib
