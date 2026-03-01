#pragma once

#include <cstdint>
#include <ostream>

namespace eyelib {
namespace physics {

/**
 * Strong handle for a physics rigid body. Default-constructed = invalid.
 * Only the physics backend should create valid instances via FromRaw().
 */
class PhysicsBodyId {
public:
    PhysicsBodyId() = default;

    bool IsValid() const { return m_value != 0; }
    explicit operator bool() const { return IsValid(); }

    uint32_t Raw() const { return m_value; }
    static PhysicsBodyId FromRaw(uint32_t v) { PhysicsBodyId id; id.m_value = v; return id; }

    bool operator==(const PhysicsBodyId& o) const { return m_value == o.m_value; }
    bool operator!=(const PhysicsBodyId& o) const { return m_value != o.m_value; }

    friend std::ostream& operator<<(std::ostream& os, const PhysicsBodyId& id) {
        return os << id.m_value;
    }

private:
    uint32_t m_value = 0;
};

/**
 * Strong handle for a collision shape. Default-constructed = invalid.
 * Only the physics backend should create valid instances via FromRaw().
 */
class PhysicsShapeId {
public:
    PhysicsShapeId() = default;

    bool IsValid() const { return m_value != 0; }
    explicit operator bool() const { return IsValid(); }

    uint32_t Raw() const { return m_value; }
    static PhysicsShapeId FromRaw(uint32_t v) { PhysicsShapeId id; id.m_value = v; return id; }

    bool operator==(const PhysicsShapeId& o) const { return m_value == o.m_value; }
    bool operator!=(const PhysicsShapeId& o) const { return m_value != o.m_value; }

    friend std::ostream& operator<<(std::ostream& os, const PhysicsShapeId& id) {
        return os << id.m_value;
    }

private:
    uint32_t m_value = 0;
};

} // namespace physics
} // namespace eyelib
