#pragma once

#include "eyelib/math/AABB.h"
#include "eyelib/math/Types.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace eyelib {
namespace physics {

/**
 * Shape storage for the Jolt backend. Creates and owns JPH shapes (Box, Sphere, Capsule).
 * GetShape(handle) returns the JPH::Shape* for body creation (internal use only).
 */
class JoltShapeStore {
public:
    using Handle = uint32_t;

    JoltShapeStore();
    ~JoltShapeStore();

    Handle CreateBox(const eyelib::math::Vec3& halfExtents);
    Handle CreateSphere(float radius);
    Handle CreateCapsule(float radius, float height);
    void Destroy(Handle shape);
    bool IsValid(Handle shape) const;

    /** Local-space AABB. */
    std::optional<eyelib::math::AABB> GetLocalAABB(Handle shape) const;

    /** Returns the JPH Shape pointer for this handle. Only valid if IsValid(shape). For backend use. */
    const void* GetShapePointer(Handle shape) const;

    void Clear();

private:
    struct Impl;
    Impl* m_impl = nullptr;

    static Handle ToHandle(size_t oneBased);
    static size_t ToIndex(Handle h);
};

} // namespace physics
} // namespace eyelib
