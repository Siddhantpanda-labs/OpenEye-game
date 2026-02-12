#include "Time.h"
#include "eyelib/core/Time.h"

namespace eyelib {
namespace api {

void Time::Initialize() {
    eyelib::core::Time::Initialize();
}

void Time::Update() {
    eyelib::core::Time::Update();
}

float Time::DeltaTime() {
    return eyelib::core::Time::DeltaTime();
}

float Time::TotalTime() {
    return eyelib::core::Time::TotalTime();
}

uint64_t Time::FrameCount() {
    return eyelib::core::Time::FrameCount();
}

float Time::FPS() {
    return eyelib::core::Time::FPS();
}

float Time::FixedDeltaTime() {
    return eyelib::core::Time::FixedDeltaTime();
}

void Time::SetFixedDeltaTime(float dt) {
    eyelib::core::Time::SetFixedDeltaTime(dt);
}

float Time::SmoothedDeltaTime() {
    return eyelib::core::Time::SmoothedDeltaTime();
}

void Time::SetSmoothedDeltaAlpha(float alpha) {
    eyelib::core::Time::SetSmoothedDeltaAlpha(alpha);
}

} // namespace api
} // namespace eyelib
