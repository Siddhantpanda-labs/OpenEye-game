#include "api/animation/KeyframeEditor.h"
#include "eyelib/animation/KeyframeEditor.h"

namespace eyelib {
namespace api {

namespace {
eyelib::animation::KeyframeEditor::Track ToInternal(KeyframeEditor::Track track) {
    using InternalTrack = eyelib::animation::KeyframeEditor::Track;
    switch (track) {
        case KeyframeEditor::Track::Position: return InternalTrack::Position;
        case KeyframeEditor::Track::Rotation: return InternalTrack::Rotation;
        case KeyframeEditor::Track::Scale: return InternalTrack::Scale;
    }
    return InternalTrack::Position;
}
} // namespace

void KeyframeEditor::AddKeyframe(AnimationClip& clip, int boneIndex, float time, const math::Vec3& value, Track track) {
    eyelib::animation::KeyframeEditor::AddKeyframe(*clip.GetInternal(), boneIndex, time, value, ToInternal(track));
}

void KeyframeEditor::AddKeyframe(AnimationClip& clip, int boneIndex, float time, const math::Quat& value) {
    eyelib::animation::KeyframeEditor::AddKeyframe(*clip.GetInternal(), boneIndex, time, value);
}

void KeyframeEditor::RemoveKeyframe(AnimationClip& clip, int boneIndex, float time, Track track) {
    eyelib::animation::KeyframeEditor::RemoveKeyframe(*clip.GetInternal(), boneIndex, time, ToInternal(track));
}

void KeyframeEditor::SetKeyframeValue(AnimationClip& clip, int boneIndex, float time, const math::Vec3& value, Track track) {
    eyelib::animation::KeyframeEditor::SetKeyframeValue(*clip.GetInternal(), boneIndex, time, value, ToInternal(track));
}

void KeyframeEditor::SetKeyframeValue(AnimationClip& clip, int boneIndex, float time, const math::Quat& value) {
    eyelib::animation::KeyframeEditor::SetKeyframeValue(*clip.GetInternal(), boneIndex, time, value);
}

size_t KeyframeEditor::GetKeyframeCount(const AnimationClip& clip, int boneIndex) {
    return eyelib::animation::KeyframeEditor::GetKeyframeCount(*clip.GetInternal(), boneIndex);
}

} // namespace api
} // namespace eyelib
