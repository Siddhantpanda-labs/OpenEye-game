#include "eyelib/physics/softbody/SoftBodyFactory.h"
#include "eyelib/core/Log.h"

#include <BulletSoftBody/btSoftBody.h>
#include <BulletSoftBody/btSoftBodyHelpers.h>

namespace eyelib {
namespace physics {

namespace {
btVector3 ToBt(const math::Vec3& v) { return btVector3(v.x, v.y, v.z); }
math::Vec3 ToGlm(const btVector3& v) { return math::Vec3(v.x(), v.y(), v.z()); }
} // namespace

SoftBodyHandle SoftBodyFactory::CreateCloth(
    PhysicsWorld* world,
    const math::Vec3& position,
    int widthSegments,
    int heightSegments,
    float width,
    float height,
    float mass) {
    if (!world) return 0;
    btSoftBodyWorldInfo& info = world->GetSoftBodyWorldInfo();

    btVector3 c00(-width * 0.5f, 0.0f, 0.0f);
    btVector3 c10(+width * 0.5f, 0.0f, 0.0f);
    btVector3 c01(-width * 0.5f, -height, 0.0f);
    btVector3 c11(+width * 0.5f, -height, 0.0f);

    btSoftBody* cloth = btSoftBodyHelpers::CreatePatch(
        info, c00, c10, c01, c11, widthSegments, heightSegments, 0, true);

    if (!cloth) return 0;
    cloth->getCollisionShape()->setMargin(0.02f);
    cloth->generateBendingConstraints(2, cloth->appendMaterial());
    cloth->setTotalMass(mass);
    cloth->m_cfg.kDP = 0.01f;
    cloth->m_cfg.kDF = 0.1f;
    cloth->m_cfg.piterations = 10;
    cloth->m_cfg.diterations = 10;
    cloth->m_cfg.citerations = 10;
    cloth->m_cfg.collisions = btSoftBody::fCollision::SDF_RD |
                              btSoftBody::fCollision::SDF_RDN |
                              btSoftBody::fCollision::SDF_RDF;
    cloth->setSelfCollision(false);

    btTransform t;
    t.setIdentity();
    t.setOrigin(ToBt(position));
    cloth->transform(t);

    return world->AddSoftBody(cloth);
}

SoftBodyHandle SoftBodyFactory::CreateRope(
    PhysicsWorld* world,
    const math::Vec3& startPos,
    const math::Vec3& endPos,
    int segments,
    float mass) {
    if (!world) return 0;
    btSoftBodyWorldInfo& info = world->GetSoftBodyWorldInfo();
    btSoftBody* rope = btSoftBodyHelpers::CreateRope(
        info, ToBt(startPos), ToBt(endPos), segments, 1);
    if (!rope) return 0;
    rope->setTotalMass(mass);
    rope->m_cfg.kDP = 0.1f;
    rope->m_cfg.piterations = 10;
    return world->AddSoftBody(rope);
}

SoftBodyHandle SoftBodyFactory::CreateVolumeFromMesh(
    PhysicsWorld* world,
    const std::vector<math::Vec3>& vertices,
    const std::vector<int>& indices,
    const math::Vec3& position,
    float mass) {
    if (!world || vertices.empty() || indices.empty()) return 0;
    btSoftBodyWorldInfo& info = world->GetSoftBodyWorldInfo();

    std::vector<float> v;
    v.reserve(vertices.size() * 3);
    for (const auto& p : vertices) {
        v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
    }
    btSoftBody* body = btSoftBodyHelpers::CreateFromTriMesh(
        info, v.data(), indices.data(), static_cast<int>(indices.size() / 3), true);
    if (!body) return 0;

    body->getCollisionShape()->setMargin(0.05f);
    body->setTotalMass(mass, true);
    body->generateBendingConstraints(2, body->appendMaterial());
    body->m_cfg.kDP = 0.02f;
    body->m_cfg.kDF = 0.3f;
    body->m_cfg.piterations = 10;
    body->m_cfg.viterations = 10;
    body->m_cfg.diterations = 10;
    body->m_cfg.collisions = btSoftBody::fCollision::SDF_RD |
                             btSoftBody::fCollision::SDF_RDN |
                             btSoftBody::fCollision::SDF_RDF;

    btTransform t;
    t.setIdentity();
    t.setOrigin(ToBt(position));
    body->transform(t);
    return world->AddSoftBody(body);
}

SoftBodyHandle SoftBodyFactory::CreateVolumeSphere(
    PhysicsWorld* world,
    const math::Vec3& position,
    float radius,
    int resolution,
    float mass,
    float pressure) {
    if (!world || radius <= 0.0f) return 0;
    btSoftBodyWorldInfo& info = world->GetSoftBodyWorldInfo();
    btSoftBody* body = btSoftBodyHelpers::CreateEllipsoid(
        info, ToBt(position), btVector3(radius, radius, radius), resolution);
    if (!body) return 0;

    body->getCollisionShape()->setMargin(0.05f);
    body->setTotalMass(mass, true);
    body->generateBendingConstraints(2, body->appendMaterial());
    body->m_cfg.kPR = btScalar(pressure);
    body->m_cfg.kVC = btScalar(20.0f);
    body->m_cfg.kDP = btScalar(0.02f);
    body->m_cfg.kDF = btScalar(0.3f);
    body->m_cfg.piterations = 15;
    body->m_cfg.viterations = 15;
    body->m_cfg.diterations = 10;
    body->m_cfg.collisions = btSoftBody::fCollision::SDF_RD |
                             btSoftBody::fCollision::SDF_RDN |
                             btSoftBody::fCollision::SDF_RDF;
    return world->AddSoftBody(body);
}

void SoftBodyFactory::SetMass(PhysicsWorld* world, SoftBodyHandle body, float mass) {
    if (!world) return;
    btSoftBody* b = world->GetSoftBodyInternal(body);
    if (b) b->setTotalMass(mass, true);
}

void SoftBodyFactory::SetStiffness(PhysicsWorld* world, SoftBodyHandle body, float stiffness) {
    if (!world) return;
    btSoftBody* b = world->GetSoftBodyInternal(body);
    if (b) b->m_materials[0]->m_kLST = stiffness;
}

void SoftBodyFactory::SetDamping(PhysicsWorld* world, SoftBodyHandle body, float damping) {
    if (!world) return;
    btSoftBody* b = world->GetSoftBodyInternal(body);
    if (b) b->m_cfg.kDP = btScalar(damping);
}

void SoftBodyFactory::AttachToRigidBody(PhysicsWorld* world, SoftBodyHandle body, RigidBodyHandle rigidBody,
                                        int nodeIndex, bool disableCollision, float influence) {
    if (!world) return;
    btSoftBody* b = world->GetSoftBodyInternal(body);
    btRigidBody* rb = world->GetRigidBodyInternal(rigidBody);
    if (!b || !rb) return;
    if (nodeIndex < 0 || nodeIndex >= b->m_nodes.size()) return;
    b->appendAnchor(nodeIndex, rb, disableCollision, influence);
}

math::Vec3 SoftBodyFactory::GetNodePosition(PhysicsWorld* world, SoftBodyHandle body, int nodeIndex) {
    if (!world) return math::Vec3(0.f);
    btSoftBody* b = world->GetSoftBodyInternal(body);
    if (!b || nodeIndex < 0 || nodeIndex >= b->m_nodes.size()) return math::Vec3(0.f);
    return ToGlm(b->m_nodes[nodeIndex].m_x);
}

void SoftBodyFactory::SetNodePosition(PhysicsWorld* world, SoftBodyHandle body, int nodeIndex, const math::Vec3& position) {
    if (!world) return;
    btSoftBody* b = world->GetSoftBodyInternal(body);
    if (!b || nodeIndex < 0 || nodeIndex >= b->m_nodes.size()) return;
    b->m_nodes[nodeIndex].m_x = ToBt(position);
}

void SoftBodyFactory::ApplyForce(PhysicsWorld* world, SoftBodyHandle body, int nodeIndex, const math::Vec3& force) {
    if (!world) return;
    btSoftBody* b = world->GetSoftBodyInternal(body);
    if (!b || nodeIndex < 0 || nodeIndex >= b->m_nodes.size()) return;
    b->addForce(ToBt(force), nodeIndex);
}

void SoftBodyFactory::Destroy(PhysicsWorld* world, SoftBodyHandle body) {
    if (world) world->RemoveSoftBody(body);
}

} // namespace physics
} // namespace eyelib
