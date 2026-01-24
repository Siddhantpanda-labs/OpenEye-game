#include "eyelib/physics/softbody/Cloth.h"
#include <BulletSoftBody/btSoftBody.h>

namespace eyelib {
namespace physics {

void Cloth::SetBending(PhysicsWorld* world, SoftBodyHandle cloth, int distance, float stiffness) {
    if (!world) return;
    btSoftBody* b = world->GetSoftBodyInternal(cloth);
    if (!b) return;
    b->generateBendingConstraints(distance, b->appendMaterial());
    b->m_materials[0]->m_kLST = stiffness;
}

void Cloth::SetWind(PhysicsWorld* world, SoftBodyHandle cloth, const math::Vec3& direction, float strength) {
    if (!world) return;
    btSoftBody* b = world->GetSoftBodyInternal(cloth);
    if (!b) return;
    btVector3 wind(direction.x, direction.y, direction.z);
    b->setWindVelocity(wind * strength);
}

void Cloth::SetCollision(PhysicsWorld* world, SoftBodyHandle cloth, bool enabled) {
    if (!world) return;
    btSoftBody* b = world->GetSoftBodyInternal(cloth);
    if (!b) return;
    if (enabled) {
        b->m_cfg.collisions |= btSoftBody::fCollision::SDF_RD;
        b->m_cfg.collisions |= btSoftBody::fCollision::SDF_RDN;
        b->m_cfg.collisions |= btSoftBody::fCollision::SDF_RDF;
    } else {
        b->m_cfg.collisions = 0;
    }
}

void Cloth::SetSelfCollision(PhysicsWorld* world, SoftBodyHandle cloth, bool enabled) {
    if (!world) return;
    btSoftBody* b = world->GetSoftBodyInternal(cloth);
    if (!b) return;
    if (enabled) {
        b->m_cfg.collisions |= btSoftBody::fCollision::SDF_RD;
        b->m_cfg.collisions |= btSoftBody::fCollision::SDF_RDN;
        b->m_cfg.collisions |= btSoftBody::fCollision::SDF_RDF;
    } else {
        b->m_cfg.collisions = 0;
    }
}

void Cloth::SetDamping(PhysicsWorld* world, SoftBodyHandle cloth, float damping) {
    SoftBodyFactory::SetDamping(world, cloth, damping);
}

} // namespace physics
} // namespace eyelib
