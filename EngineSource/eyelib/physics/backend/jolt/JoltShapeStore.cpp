#include "JoltShapeStore.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Geometry/AABox.h>

JPH_SUPPRESS_WARNINGS

namespace eyelib {
namespace physics {

struct JoltShapeStore::Impl {
    std::vector<JPH::ShapeRefC> entries;
};

JoltShapeStore::Handle JoltShapeStore::ToHandle(size_t oneBased) {
    return static_cast<uint32_t>(oneBased);
}
size_t JoltShapeStore::ToIndex(Handle h) {
    return h > 0 ? static_cast<size_t>(h - 1) : 0;
}

JoltShapeStore::JoltShapeStore() : m_impl(new Impl()) {}

JoltShapeStore::~JoltShapeStore() {
    delete m_impl;
    m_impl = nullptr;
}

JoltShapeStore::Handle JoltShapeStore::CreateBox(const eyelib::math::Vec3& halfExtents) {
    JPH::BoxShapeSettings settings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
    JPH::ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return 0;
    JPH::ShapeRefC shape = result.Get();
    m_impl->entries.push_back(shape);
    return ToHandle(m_impl->entries.size());
}

JoltShapeStore::Handle JoltShapeStore::CreateSphere(float radius) {
    JPH::SphereShapeSettings settings(radius);
    JPH::ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return 0;
    JPH::ShapeRefC shape = result.Get();
    m_impl->entries.push_back(shape);
    return ToHandle(m_impl->entries.size());
}

JoltShapeStore::Handle JoltShapeStore::CreateCapsule(float radius, float height) {
    float halfHeight = (height * 0.5f) - radius;
    if (halfHeight < 0.0f) halfHeight = 0.0f;
    JPH::CapsuleShapeSettings settings(halfHeight, radius);
    JPH::ShapeSettings::ShapeResult result = settings.Create();
    if (result.HasError()) return 0;
    JPH::ShapeRefC shape = result.Get();
    m_impl->entries.push_back(shape);
    return ToHandle(m_impl->entries.size());
}

void JoltShapeStore::Destroy(Handle shape) {
    if (shape == 0 || shape > m_impl->entries.size()) return;
    m_impl->entries[ToIndex(shape)] = nullptr;
}

bool JoltShapeStore::IsValid(Handle shape) const {
    if (shape == 0 || shape > m_impl->entries.size()) return false;
    return m_impl->entries[ToIndex(shape)] != nullptr;
}

std::optional<eyelib::math::AABB> JoltShapeStore::GetLocalAABB(Handle shape) const {
    if (!IsValid(shape)) return std::nullopt;
    const JPH::Shape* s = m_impl->entries[ToIndex(shape)].GetPtr();
    if (!s) return std::nullopt;
    JPH::AABox box = s->GetLocalBounds();
    eyelib::math::AABB aabb;
    aabb.min.x = box.mMin.GetX();
    aabb.min.y = box.mMin.GetY();
    aabb.min.z = box.mMin.GetZ();
    aabb.max.x = box.mMax.GetX();
    aabb.max.y = box.mMax.GetY();
    aabb.max.z = box.mMax.GetZ();
    return aabb;
}

const void* JoltShapeStore::GetShapePointer(Handle shape) const {
    if (!IsValid(shape)) return nullptr;
    return m_impl->entries[ToIndex(shape)].GetPtr();
}

void JoltShapeStore::Clear() {
    m_impl->entries.clear();
}

} // namespace physics
} // namespace eyelib
