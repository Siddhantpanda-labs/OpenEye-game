#pragma once

// JoltWorld: owns Jolt PhysicsSystem, layers, job system, temp allocator.
// Single-threaded; used only by JoltBackend. All JPH types stay in .cpp.

#include "eyelib/math/Types.h"

#include <functional>

namespace eyelib {
namespace physics {

class JoltWorld {
public:
    JoltWorld();
    ~JoltWorld();

    JoltWorld(const JoltWorld&) = delete;
    JoltWorld& operator=(const JoltWorld&) = delete;

    /** Call once before creating any JoltWorld (or objects that use JPH::Allocate). Required for demo/tests. */
    static void EnsureJoltGlobalInit();

    /** One-time Jolt init and physics system setup. Returns false on failure. */
    bool Init();
    /** Call before shutdown (UnregisterTypes, destroy factory). */
    void Shutdown();

    bool IsInitialized() const { return m_initialized; }

    /** Per physics_advice §5: state for assert-on-use. */
    enum class State { Uninitialized, Running, ShuttingDown, Destroyed };
    State GetState() const { return m_state; }

    void SetGravity(const eyelib::math::Vec3& gravity);
    eyelib::math::Vec3 GetGravity() const;

    /** Step physics. inCollisionSteps typically 1 per fixed step (e.g. 1 for 60Hz). */
    void Update(float deltaTime, int collisionSteps);

    /** Opaque pointers for backend use. Cast to JPH::BodyInterface* and JPH::NarrowPhaseQuery*. */
    void* GetBodyInterface();
    const void* GetBodyInterface() const;
    const void* GetNarrowPhaseQuery() const;
    const void* GetBroadPhaseQuery() const;
    void* GetPhysicsSystem();
    const void* GetPhysicsSystem() const;

    /** Set optional contact callback (bodyHandleA, bodyHandleB, position, normal). */
    using ContactCallback = std::function<void(uint32_t, uint32_t, const eyelib::math::Vec3&, const eyelib::math::Vec3&)>;
    void SetContactCallback(ContactCallback cb);
    ContactCallback GetContactCallback() const { return m_contactCb; }

private:
    void DrainJobSystem();

    bool m_initialized = false;
    State m_state = State::Uninitialized;
    ContactCallback m_contactCb;

    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace physics
} // namespace eyelib
