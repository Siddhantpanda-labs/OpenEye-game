#include "eyelib/physics/PhysicsGlobals.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>

JPH_SUPPRESS_WARNINGS

namespace eyelib {
namespace physics {

namespace {

static bool s_joltGlobalsInitialized = false;

static void JoltTraceStub(const char* /*inFMT*/, ...) {}

#ifdef JPH_ENABLE_ASSERTS
static bool JoltAssertStub(const char* /*inExpression*/, const char* /*inMessage*/,
                          const char* /*inFile*/, unsigned int /*inLine*/) {
    return true;
}
#endif

} // namespace

void EnsureJoltGlobalsInitialized() {
    if (s_joltGlobalsInitialized) return;
    JPH::RegisterDefaultAllocator();
    JPH::Trace = JoltTraceStub;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = JoltAssertStub);
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    s_joltGlobalsInitialized = true;
}

void ShutdownJoltGlobals() {
    if (!s_joltGlobalsInitialized) return;
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
    s_joltGlobalsInitialized = false;
}

} // namespace physics
} // namespace eyelib
