#include "api/physics/SoftBody.h"

namespace eyelib {
namespace api {
namespace physics {

SoftBodyHandle SoftBody::CreateCloth(PhysicsWorld& /*world*/, const eyelib::math::Vec3& /*position*/,
                                    int /*widthSegments*/, int /*heightSegments*/,
                                    float /*width*/, float /*height*/, float /*mass*/) {
    return 0;
}

SoftBodyHandle SoftBody::CreateRope(PhysicsWorld& /*world*/, const eyelib::math::Vec3& /*startPos*/,
                                   const eyelib::math::Vec3& /*endPos*/, int /*segments*/, float /*mass*/) {
    return 0;
}

SoftBodyHandle SoftBody::CreateVolumeFromMesh(PhysicsWorld& /*world*/,
                                              const std::vector<eyelib::math::Vec3>& /*vertices*/,
                                              const std::vector<int>& /*indices*/,
                                              const eyelib::math::Vec3& /*position*/,
                                              float /*mass*/) {
    return 0;
}

SoftBodyHandle SoftBody::CreateVolumeSphere(PhysicsWorld& /*world*/, const eyelib::math::Vec3& /*position*/,
                                            float /*radius*/, int /*resolution*/, float /*mass*/, float /*pressure*/) {
    return 0;
}

void SoftBody::SetMass(PhysicsWorld& /*world*/, SoftBodyHandle /*body*/, float /*mass*/) {}
void SoftBody::SetStiffness(PhysicsWorld& /*world*/, SoftBodyHandle /*body*/, float /*stiffness*/) {}
void SoftBody::SetDamping(PhysicsWorld& /*world*/, SoftBodyHandle /*body*/, float /*damping*/) {}
void SoftBody::SetMaterialStiffness(PhysicsWorld& /*world*/, SoftBodyHandle /*body*/, int /*materialIndex*/, float /*stiffness*/) {}
void SoftBody::SetNodeMass(PhysicsWorld& /*world*/, SoftBodyHandle /*body*/, int /*nodeIndex*/, float /*mass*/) {}
void SoftBody::SetCollisionFilter(PhysicsWorld& /*world*/, SoftBodyHandle /*body*/, short /*group*/, short /*mask*/) {}

void SoftBody::AttachToRigidBody(PhysicsWorld& /*world*/, SoftBodyHandle /*body*/, RigidBodyHandle /*rigidBody*/,
                                int /*nodeIndex*/, bool /*disableCollision*/, float /*influence*/) {}

eyelib::math::Vec3 SoftBody::GetNodePosition(PhysicsWorld& /*world*/, SoftBodyHandle /*body*/, int /*nodeIndex*/) {
    return eyelib::math::Vec3(0.f);
}

void SoftBody::SetNodePosition(PhysicsWorld& /*world*/, SoftBodyHandle /*body*/, int /*nodeIndex*/, const eyelib::math::Vec3& /*position*/) {}

void SoftBody::ApplyForce(PhysicsWorld& /*world*/, SoftBodyHandle /*body*/, int /*nodeIndex*/, const eyelib::math::Vec3& /*force*/) {}

void SoftBody::Destroy(PhysicsWorld& /*world*/, SoftBodyHandle /*body*/) {}

void SoftBody::ClothSetWind(PhysicsWorld& /*world*/, SoftBodyHandle /*cloth*/,
                           const eyelib::math::Vec3& /*direction*/, float /*strength*/) {}

void SoftBody::ClothSetCollision(PhysicsWorld& /*world*/, SoftBodyHandle /*cloth*/, bool /*enabled*/) {}

void SoftBody::ClothSetSelfCollision(PhysicsWorld& /*world*/, SoftBodyHandle /*cloth*/, bool /*enabled*/) {}

} // namespace physics
} // namespace api
} // namespace eyelib
