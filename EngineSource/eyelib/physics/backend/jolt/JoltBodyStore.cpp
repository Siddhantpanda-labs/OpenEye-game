#include "JoltBodyStore.h"

#include <cmath>

namespace eyelib {
namespace physics {

JoltBodyStore::Handle JoltBodyStore::ToHandle(size_t oneBased) {
    return static_cast<uint32_t>(oneBased);
}
size_t JoltBodyStore::ToIndex(Handle h) {
    return h > 0 ? static_cast<size_t>(h - 1) : 0;
}

JoltBodyStore::Entry* JoltBodyStore::Get(Handle body) {
    if (body == 0 || body > m_entries.size()) return nullptr;
    Entry& e = m_entries[ToIndex(body)];
    return e.alive ? &e : nullptr;
}
const JoltBodyStore::Entry* JoltBodyStore::Get(Handle body) const {
    if (body == 0 || body > m_entries.size()) return nullptr;
    const Entry& e = m_entries[ToIndex(body)];
    return e.alive ? &e : nullptr;
}

JoltBodyStore::Handle JoltBodyStore::Create(const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation,
                                            uint32_t shapeHandle, float mass) {
    Entry e;
    e.position = position;
    e.rotation = rotation;
    e.mass = mass > 0.0f ? mass : 1.0f;
    e.kinematic = (mass <= 0.0f);
    e.shapeHandle = shapeHandle;
    e.alive = true;
    m_entries.push_back(e);
    return ToHandle(m_entries.size());
}

void JoltBodyStore::Destroy(Handle body) {
    if (body == 0 || body > m_entries.size()) return;
    m_entries[ToIndex(body)].alive = false;
}

std::optional<JoltBodyStore::Transform> JoltBodyStore::GetTransform(Handle body) const {
    const Entry* e = Get(body);
    if (!e) return std::nullopt;
    Transform t;
    t.position = e->position;
    t.rotation = e->rotation;
    return t;
}

bool JoltBodyStore::SetTransform(Handle body, const eyelib::math::Vec3& position, const eyelib::math::Quat& rotation) {
    Entry* e = Get(body);
    if (!e) return false;
    e->position = position;
    e->rotation = rotation;
    return true;
}

eyelib::math::Vec3 JoltBodyStore::GetVelocity(Handle body) const {
    const Entry* e = Get(body);
    return e ? e->linearVelocity : eyelib::math::Vec3(0.0f);
}
void JoltBodyStore::SetVelocity(Handle body, const eyelib::math::Vec3& velocity) {
    Entry* e = Get(body);
    if (e) e->linearVelocity = velocity;
}
eyelib::math::Vec3 JoltBodyStore::GetAngularVelocity(Handle body) const {
    const Entry* e = Get(body);
    return e ? e->angularVelocity : eyelib::math::Vec3(0.0f);
}
void JoltBodyStore::SetAngularVelocity(Handle body, const eyelib::math::Vec3& omega) {
    Entry* e = Get(body);
    if (e) e->angularVelocity = omega;
}

void JoltBodyStore::ApplyForce(Handle body, const eyelib::math::Vec3& force, const eyelib::math::Vec3& /*point*/) {
    Entry* e = Get(body);
    if (e && !e->kinematic) e->accumulatedForce += force;
}
void JoltBodyStore::ApplyImpulse(Handle body, const eyelib::math::Vec3& impulse, const eyelib::math::Vec3& /*point*/) {
    Entry* e = Get(body);
    if (e && !e->kinematic && e->mass > 0.0f)
        e->linearVelocity += impulse * (1.0f / e->mass);
}

void JoltBodyStore::SetMass(Handle body, float mass) {
    Entry* e = Get(body);
    if (e) e->mass = mass;
}
float JoltBodyStore::GetMass(Handle body) const {
    const Entry* e = Get(body);
    return e ? e->mass : 0.0f;
}
void JoltBodyStore::SetLinearDamping(Handle body, float damping) {
    Entry* e = Get(body);
    if (e) e->linearDamping = damping;
}
void JoltBodyStore::SetAngularDamping(Handle body, float damping) {
    Entry* e = Get(body);
    if (e) e->angularDamping = damping;
}
void JoltBodyStore::SetKinematic(Handle body, bool kinematic) {
    Entry* e = Get(body);
    if (e) e->kinematic = kinematic;
}
bool JoltBodyStore::IsKinematic(Handle body) const {
    const Entry* e = Get(body);
    return e && e->kinematic;
}

void JoltBodyStore::Step(const eyelib::math::Vec3& gravity, float dt) {
    for (Entry& b : m_entries) {
        if (!b.alive || b.kinematic) continue;

        if (b.mass > 0.0f)
            b.linearVelocity += (b.accumulatedForce * (1.0f / b.mass)) * dt;
        b.accumulatedForce = eyelib::math::Vec3(0.0f);

        b.position += b.linearVelocity * dt;
        if (b.useGravity)
            b.linearVelocity += gravity * dt;

        b.linearVelocity *= (1.0f - b.linearDamping * dt);
        b.angularVelocity *= (1.0f - b.angularDamping * dt);

        const float halfDt = 0.5f * dt;
        eyelib::math::Quat omegaQ(0.0f, b.angularVelocity.x, b.angularVelocity.y, b.angularVelocity.z);
        eyelib::math::Quat dq = omegaQ * b.rotation;
        b.rotation.x += halfDt * dq.x;
        b.rotation.y += halfDt * dq.y;
        b.rotation.z += halfDt * dq.z;
        b.rotation.w += halfDt * dq.w;
        const float len2 = b.rotation.x * b.rotation.x + b.rotation.y * b.rotation.y +
                           b.rotation.z * b.rotation.z + b.rotation.w * b.rotation.w;
        if (len2 > 1e-10f) {
            const float invLen = 1.0f / std::sqrt(len2);
            b.rotation.x *= invLen;
            b.rotation.y *= invLen;
            b.rotation.z *= invLen;
            b.rotation.w *= invLen;
        }
    }
}

size_t JoltBodyStore::GetAliveCount() const {
    size_t n = 0;
    for (const auto& b : m_entries)
        if (b.alive) ++n;
    return n;
}

uint32_t JoltBodyStore::GetShapeHandle(Handle body) const {
    const Entry* e = Get(body);
    return e ? e->shapeHandle : 0;
}

void JoltBodyStore::VisitAliveBodies(std::function<void(Handle, const Transform&, uint32_t shapeHandle)> visitor) const {
    for (size_t i = 0; i < m_entries.size(); ++i) {
        const Entry& e = m_entries[i];
        if (!e.alive) continue;
        Transform t;
        t.position = e.position;
        t.rotation = e.rotation;
        visitor(ToHandle(i + 1), t, e.shapeHandle);
    }
}

void JoltBodyStore::Clear() {
    m_entries.clear();
}

} // namespace physics
} // namespace eyelib
