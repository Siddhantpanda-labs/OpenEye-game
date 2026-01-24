#pragma once

#include "api/physics/PhysicsWorld.h"
#include "eyelib/physics/softbody/SoftBodyFactory.h"
#include "eyelib/physics/softbody/Cloth.h"
#include <vector>

namespace eyelib {
namespace api {

using SoftBodyHandle = eyelib::physics::SoftBodyHandle;

class SoftBody {
public:
    static SoftBodyHandle CreateCloth(PhysicsWorld& world, const eyelib::math::Vec3& position,
                                      int widthSegments, int heightSegments,
                                      float width, float height, float mass = 1.0f);

    static SoftBodyHandle CreateRope(PhysicsWorld& world, const eyelib::math::Vec3& startPos,
                                     const eyelib::math::Vec3& endPos, int segments, float mass = 0.1f);

    static SoftBodyHandle CreateVolumeFromMesh(PhysicsWorld& world,
                                               const std::vector<eyelib::math::Vec3>& vertices,
                                               const std::vector<int>& indices,
                                               const eyelib::math::Vec3& position,
                                               float mass = 2.0f);

    static SoftBodyHandle CreateVolumeSphere(PhysicsWorld& world, const eyelib::math::Vec3& position,
                                             float radius, int resolution = 512, float mass = 2.0f, float pressure = 150.0f);

    static void SetMass(PhysicsWorld& world, SoftBodyHandle body, float mass);
    static void SetStiffness(PhysicsWorld& world, SoftBodyHandle body, float stiffness);
    static void SetDamping(PhysicsWorld& world, SoftBodyHandle body, float damping);

    static void AttachToRigidBody(PhysicsWorld& world, SoftBodyHandle body, RigidBodyHandle rigidBody,
                                  int nodeIndex, bool disableCollision = true, float influence = 1.0f);

    static eyelib::math::Vec3 GetNodePosition(PhysicsWorld& world, SoftBodyHandle body, int nodeIndex);
    static void SetNodePosition(PhysicsWorld& world, SoftBodyHandle body, int nodeIndex, const eyelib::math::Vec3& position);
    static void ApplyForce(PhysicsWorld& world, SoftBodyHandle body, int nodeIndex, const eyelib::math::Vec3& force);

    static void Destroy(PhysicsWorld& world, SoftBodyHandle body);

    // Cloth helpers
    static void ClothSetWind(PhysicsWorld& world, SoftBodyHandle cloth,
                             const eyelib::math::Vec3& direction, float strength);
    static void ClothSetCollision(PhysicsWorld& world, SoftBodyHandle cloth, bool enabled);
    static void ClothSetSelfCollision(PhysicsWorld& world, SoftBodyHandle cloth, bool enabled);
};

} // namespace api
} // namespace eyelib
