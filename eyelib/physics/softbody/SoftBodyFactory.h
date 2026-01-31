#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/physics/PhysicsWorld.h"
#include <vector>

namespace eyelib {
namespace physics {

using SoftBodyHandle = PhysicsWorld::SoftBodyHandle;

/**
 * @brief Factory for creating and managing soft bodies (cloth, rope, volume).
 */
class SoftBodyFactory {
public:
    static SoftBodyHandle CreateCloth(
        PhysicsWorld* world,
        const math::Vec3& position,
        int widthSegments,
        int heightSegments,
        float width,
        float height,
        float mass = 1.0f);

    static SoftBodyHandle CreateRope(
        PhysicsWorld* world,
        const math::Vec3& startPos,
        const math::Vec3& endPos,
        int segments,
        float mass = 0.1f);

    static SoftBodyHandle CreateVolumeFromMesh(
        PhysicsWorld* world,
        const std::vector<math::Vec3>& vertices,
        const std::vector<int>& indices,
        const math::Vec3& position,
        float mass = 2.0f);

    static SoftBodyHandle CreateVolumeSphere(
        PhysicsWorld* world,
        const math::Vec3& position,
        float radius,
        int resolution = 512,
        float mass = 2.0f,
        float pressure = 150.0f);

    static void SetMass(PhysicsWorld* world, SoftBodyHandle body, float mass);
    static void SetStiffness(PhysicsWorld* world, SoftBodyHandle body, float stiffness);
    static void SetDamping(PhysicsWorld* world, SoftBodyHandle body, float damping);
    static void SetMaterialStiffness(PhysicsWorld* world, SoftBodyHandle body, int materialIndex, float stiffness);
    static void SetNodeMass(PhysicsWorld* world, SoftBodyHandle body, int nodeIndex, float mass);
    static void SetCollisionFilter(PhysicsWorld* world, SoftBodyHandle body, short group, short mask);

    static void AttachToRigidBody(PhysicsWorld* world, SoftBodyHandle body, RigidBodyHandle rigidBody,
                                  int nodeIndex, bool disableCollision = true, float influence = 1.0f);

    static math::Vec3 GetNodePosition(PhysicsWorld* world, SoftBodyHandle body, int nodeIndex);
    static void SetNodePosition(PhysicsWorld* world, SoftBodyHandle body, int nodeIndex, const math::Vec3& position);
    static void ApplyForce(PhysicsWorld* world, SoftBodyHandle body, int nodeIndex, const math::Vec3& force);

    static void Destroy(PhysicsWorld* world, SoftBodyHandle body);
};

} // namespace physics
} // namespace eyelib
