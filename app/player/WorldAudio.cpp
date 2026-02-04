#include "WorldAudio.h"

#include "api/world/All.h"
#include "eyelib/math/Types.h"

#include <glm/glm.hpp>

namespace openeye::player {

namespace {

eyelib::math::Vec3 ForwardFromRotation(const eyelib::math::Quat& rotation) {
    const eyelib::math::Vec3 forward(0.0f, 0.0f, -1.0f);
    return rotation * forward;
}

eyelib::math::Vec3 UpFromRotation(const eyelib::math::Quat& rotation) {
    const eyelib::math::Vec3 up(0.0f, 1.0f, 0.0f);
    return rotation * up;
}

} // namespace

bool WorldAudioBridge::Initialize() {
    if (m_ready) {
        return true;
    }
    m_ready = m_device.Initialize();
    return m_ready;
}

void WorldAudioBridge::Shutdown() {
    if (!m_ready) {
        return;
    }
    m_device.Shutdown();
    m_ready = false;
}

void WorldAudioBridge::BuildFromWorld(eyelib::api::world::World& /*world*/) {
    // No-op for now; could collect audio sources/listeners from world components later.
}

void WorldAudioBridge::Update(eyelib::api::world::World& world, float /*deltaSeconds*/) {
    if (!m_ready) {
        return;
    }
    const auto obs = world.GetObservation();
    const auto cameras = obs.GetCameras();
    const auto transforms = obs.GetTransforms();

    eyelib::world::EntityId primaryCameraEntity = eyelib::world::InvalidEntity;
    for (const auto& entry : cameras) {
        if (entry.component.isPrimary) {
            primaryCameraEntity = entry.entity;
            break;
        }
    }
    if (primaryCameraEntity == eyelib::world::InvalidEntity && !cameras.empty()) {
        primaryCameraEntity = cameras.front().entity;
    }

    if (primaryCameraEntity != eyelib::world::InvalidEntity) {
        for (const auto& entry : transforms) {
            if (entry.entity == primaryCameraEntity) {
                m_listener.SetPosition(entry.component.position);
                const eyelib::math::Vec3 forward = ForwardFromRotation(entry.component.rotation);
                const eyelib::math::Vec3 up = UpFromRotation(entry.component.rotation);
                m_listener.SetOrientation(forward, up);
                m_listener.SetVelocity(eyelib::math::Vec3(0.0f));
                break;
            }
        }
    }
}

} // namespace openeye::player
