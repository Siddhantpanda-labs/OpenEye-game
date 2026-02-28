#include "JoltWorld.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/Body.h>

JPH_SUPPRESS_WARNINGS

namespace eyelib {
namespace physics {

using namespace JPH;

static void JoltTraceImpl(const char* inFMT, ...) {
    (void)inFMT;
    // No-op trace; connect to your logger if needed.
}

#ifdef JPH_ENABLE_ASSERTS
static bool JoltAssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, unsigned int inLine) {
    (void)inExpression;
    (void)inMessage;
    (void)inFile;
    (void)inLine;
    return true; // breakpoint
}
#endif

enum class ObjectLayer : JPH::ObjectLayer {
    NON_MOVING = 0,
    MOVING = 1,
    NUM_LAYERS = 2
};

namespace BroadPhaseLayers {
    constexpr BroadPhaseLayer NON_MOVING(0);
    constexpr BroadPhaseLayer MOVING(1);
    constexpr uint NUM_LAYERS = 2;
}

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[static_cast<JPH::ObjectLayer>(ObjectLayer::NON_MOVING)] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[static_cast<JPH::ObjectLayer>(ObjectLayer::MOVING)] = BroadPhaseLayers::MOVING;
    }
    uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
    BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        return mObjectToBroadPhase[inLayer];
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override {
        (void)inLayer;
        return "Layer";
    }
#endif
private:
    BroadPhaseLayer mObjectToBroadPhase[static_cast<size_t>(ObjectLayer::NUM_LAYERS)];
};

class ObjectVsBroadPhaseLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
        (void)inLayer2;
        return inLayer1 == static_cast<JPH::ObjectLayer>(ObjectLayer::MOVING);
    }
};

class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
        if (inObject1 == static_cast<JPH::ObjectLayer>(ObjectLayer::NON_MOVING))
            return inObject2 == static_cast<JPH::ObjectLayer>(ObjectLayer::MOVING);
        if (inObject1 == static_cast<JPH::ObjectLayer>(ObjectLayer::MOVING))
            return true;
        return false;
    }
};

class ContactListenerImpl : public ContactListener {
public:
    explicit ContactListenerImpl(JoltWorld::ContactCallback cb) : m_cb(std::move(cb)) {}
    void OnContactAdded(const Body& inBody1, const Body& inBody2, const ContactManifold& inManifold, ContactSettings&) override {
        if (!m_cb) return;
        uint32_t id1 = inBody1.GetID().GetIndexAndSequenceNumber();
        uint32_t id2 = inBody2.GetID().GetIndexAndSequenceNumber();
        RVec3 pos = inManifold.GetWorldSpaceContactPointOn1(0);
        Vec3 n = inManifold.mWorldSpaceNormal;
        eyelib::math::Vec3 position(static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ()));
        eyelib::math::Vec3 normal(n.GetX(), n.GetY(), n.GetZ());
        m_cb(id1, id2, position, normal);
    }
    void OnContactPersisted(const Body&, const Body&, const ContactManifold&, ContactSettings&) override {}
    void OnContactRemoved(const SubShapeIDPair&) override {}
    void SetCallback(JoltWorld::ContactCallback cb) { m_cb = std::move(cb); }
private:
    JoltWorld::ContactCallback m_cb;
};

struct JoltWorld::Impl {
    static bool s_joltGlobalInit;

    BPLayerInterfaceImpl broadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseFilter;
    ObjectLayerPairFilterImpl objectLayerPairFilter;
    TempAllocatorImpl tempAllocator{10 * 1024 * 1024};
    JobSystemThreadPool jobSystem{1024, 1024, 1};
    PhysicsSystem physicsSystem;
    ContactListenerImpl contactListener{nullptr};

    static constexpr uint cMaxBodies = 65536;
    static constexpr uint cMaxBodyPairs = 65536;
    static constexpr uint cMaxContactConstraints = 10240;
};

bool JoltWorld::Impl::s_joltGlobalInit = false;

void JoltWorld::EnsureJoltGlobalInit() {
    if (Impl::s_joltGlobalInit) return;
    JPH::RegisterDefaultAllocator();
    JPH::Trace = JoltTraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertFailedImpl);
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    Impl::s_joltGlobalInit = true;
}

JoltWorld::JoltWorld() : m_impl(new Impl()) {}

JoltWorld::~JoltWorld() {
    Shutdown();
    delete m_impl;
    m_impl = nullptr;
}

bool JoltWorld::Init() {
    if (m_initialized) return true;

    EnsureJoltGlobalInit();

    m_impl->physicsSystem.Init(
        Impl::cMaxBodies,
        0,
        Impl::cMaxBodyPairs,
        Impl::cMaxContactConstraints,
        m_impl->broadPhaseLayerInterface,
        m_impl->objectVsBroadPhaseFilter,
        m_impl->objectLayerPairFilter
    );
    m_impl->physicsSystem.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
    m_impl->physicsSystem.SetContactListener(&m_impl->contactListener);

    m_initialized = true;
    return true;
}

void JoltWorld::Shutdown() {
    if (!m_initialized) return;
    m_impl->physicsSystem.SetContactListener(nullptr);
    m_initialized = false;
    // Do not UnregisterTypes/delete Factory here; other JoltWorld instances might be created later.
    // Call from process exit if needed.
}

void JoltWorld::SetGravity(const eyelib::math::Vec3& gravity) {
    if (!m_impl) return;
    m_impl->physicsSystem.SetGravity(JPH::Vec3(gravity.x, gravity.y, gravity.z));
}

eyelib::math::Vec3 JoltWorld::GetGravity() const {
    if (!m_impl) return eyelib::math::Vec3(0.0f, -9.81f, 0.0f);
    JPH::Vec3 g = m_impl->physicsSystem.GetGravity();
    return eyelib::math::Vec3(g.GetX(), g.GetY(), g.GetZ());
}

void JoltWorld::Update(float deltaTime, int collisionSteps) {
    if (!m_initialized || !m_impl) return;
    int steps = collisionSteps > 0 ? collisionSteps : 1;
    m_impl->physicsSystem.Update(deltaTime, steps, &m_impl->tempAllocator, &m_impl->jobSystem);
}

void* JoltWorld::GetBodyInterface() {
    return m_impl ? const_cast<JPH::BodyInterface*>(&m_impl->physicsSystem.GetBodyInterface()) : nullptr;
}
const void* JoltWorld::GetBodyInterface() const {
    return m_impl ? &m_impl->physicsSystem.GetBodyInterface() : nullptr;
}
const void* JoltWorld::GetNarrowPhaseQuery() const {
    return m_impl ? &m_impl->physicsSystem.GetNarrowPhaseQuery() : nullptr;
}
const void* JoltWorld::GetBroadPhaseQuery() const {
    return m_impl ? &m_impl->physicsSystem.GetBroadPhaseQuery() : nullptr;
}
void* JoltWorld::GetPhysicsSystem() {
    return m_impl ? &m_impl->physicsSystem : nullptr;
}
const void* JoltWorld::GetPhysicsSystem() const {
    return m_impl ? &m_impl->physicsSystem : nullptr;
}

void JoltWorld::SetContactCallback(ContactCallback cb) {
    m_contactCb = std::move(cb);
    if (m_impl)
        m_impl->contactListener.SetCallback(m_contactCb);
}

} // namespace physics
} // namespace eyelib
