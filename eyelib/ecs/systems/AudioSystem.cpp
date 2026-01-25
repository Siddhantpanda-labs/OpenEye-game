#include "eyelib/ecs/systems/AudioSystem.h"

#include "eyelib/ecs/World.h"
#include "eyelib/ecs/components/AudioListener.h"
#include "eyelib/ecs/components/AudioSource.h"
#include "eyelib/ecs/components/Transform.h"
#include "eyelib/audio/AudioDevice.h"

namespace eyelib {
namespace ecs {

namespace {
math::Vec3 ForwardFromRotation(const math::Quat& q) {
    return glm::normalize(q * math::Vec3(0.0f, 0.0f, -1.0f));
}

math::Vec3 UpFromRotation(const math::Quat& q) {
    return glm::normalize(q * math::Vec3(0.0f, 1.0f, 0.0f));
}
} // namespace

void AudioSystem::Update(World& world, float deltaTime) {
    static audio::AudioDevice s_device;
    static bool s_deviceInit = false;
    if (!s_deviceInit) {
        s_deviceInit = s_device.Initialize();
    }

    auto listenerView = world.Registry().view<components::Transform, components::AudioListener>();
    for (auto entity : listenerView) {
        auto& transform = listenerView.get<components::Transform>(entity);
        auto& listener = listenerView.get<components::AudioListener>(entity);
        if (!listener.active) continue;

        const math::Mat4 worldMat = transform.GetWorldMatrix(world);
        const math::Vec3 position = math::Vec3(worldMat[3]);
        const math::Vec3 forward = ForwardFromRotation(transform.rotation);
        const math::Vec3 up = UpFromRotation(transform.rotation);

        math::Vec3 velocity(0.0f);
        if (listener.hasLast && deltaTime > 0.0f) {
            velocity = (position - listener.lastPosition) / deltaTime;
        }
        listener.lastPosition = position;
        listener.hasLast = true;

        listener.listener.SetPosition(position);
        listener.listener.SetOrientation(forward, up);
        listener.listener.SetVelocity(velocity);
    }

    auto sourceView = world.Registry().view<components::Transform, components::AudioSource>();
    for (auto entity : sourceView) {
        auto& transform = sourceView.get<components::Transform>(entity);
        auto& sourceComp = sourceView.get<components::AudioSource>(entity);

        const math::Vec3 position = transform.position;
        math::Vec3 velocity(0.0f);
        if (sourceComp.hasLast && deltaTime > 0.0f) {
            velocity = (position - sourceComp.lastPosition) / deltaTime;
        }
        sourceComp.lastPosition = position;
        sourceComp.hasLast = true;

        sourceComp.source.SetPosition(position);
        sourceComp.source.SetVelocity(velocity);
        sourceComp.source.SetVolume(sourceComp.volume);
        sourceComp.source.SetPitch(sourceComp.pitch);

        if (sourceComp.streaming) {
            if (!sourceComp.started && !sourceComp.streamPath.empty()) {
                sourceComp.source.PlayStream(sourceComp.streamPath, sourceComp.streamLoop);
                sourceComp.started = true;
            }
            sourceComp.source.UpdateStream();
        } else {
            sourceComp.source.SetLooping(sourceComp.looping);
            if (sourceComp.playOnStart && !sourceComp.started && sourceComp.clip) {
                sourceComp.source.Play(*sourceComp.clip);
                sourceComp.started = true;
            }
        }
    }
}

} // namespace ecs
} // namespace eyelib
