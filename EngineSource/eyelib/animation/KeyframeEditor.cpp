#include "KeyframeEditor.h"

#include <algorithm>
#include <cmath>

namespace eyelib {
namespace animation {

namespace {
constexpr float kTimeEpsilon = 1e-4f;

template <typename T>
void InsertKey(std::vector<Keyframe<T>>& keys, float time, const T& value) {
    keys.push_back(Keyframe<T>{ time, value });
    std::sort(keys.begin(), keys.end(), [](const Keyframe<T>& a, const Keyframe<T>& b) {
        return a.time < b.time;
    });
}

template <typename T>
bool UpdateKey(std::vector<Keyframe<T>>& keys, float time, const T& value) {
    for (auto& k : keys) {
        if (std::fabs(k.time - time) <= kTimeEpsilon) {
            k.value = value;
            return true;
        }
    }
    return false;
}

template <typename T>
void RemoveKey(std::vector<Keyframe<T>>& keys, float time) {
    keys.erase(std::remove_if(keys.begin(), keys.end(), [time](const Keyframe<T>& k) {
        return std::fabs(k.time - time) <= kTimeEpsilon;
    }), keys.end());
}
} // namespace

void KeyframeEditor::AddKeyframe(AnimationClip& clip, int boneIndex, float time, const math::Vec3& value, Track track) {
    auto* ch = clip.FindOrCreateChannel(boneIndex);
    if (!ch) return;
    if (track == Track::Position) {
        InsertKey(ch->positionKeys, time, value);
    } else if (track == Track::Scale) {
        InsertKey(ch->scaleKeys, time, value);
    }
}

void KeyframeEditor::AddKeyframe(AnimationClip& clip, int boneIndex, float time, const math::Quat& value) {
    auto* ch = clip.FindOrCreateChannel(boneIndex);
    if (!ch) return;
    InsertKey(ch->rotationKeys, time, value);
}

void KeyframeEditor::RemoveKeyframe(AnimationClip& clip, int boneIndex, float time, Track track) {
    auto* ch = clip.FindOrCreateChannel(boneIndex);
    if (!ch) return;
    if (track == Track::Position) {
        RemoveKey(ch->positionKeys, time);
    } else if (track == Track::Scale) {
        RemoveKey(ch->scaleKeys, time);
    } else {
        RemoveKey(ch->rotationKeys, time);
    }
}

void KeyframeEditor::SetKeyframeValue(AnimationClip& clip, int boneIndex, float time, const math::Vec3& value, Track track) {
    auto* ch = clip.FindOrCreateChannel(boneIndex);
    if (!ch) return;
    bool updated = false;
    if (track == Track::Position) {
        updated = UpdateKey(ch->positionKeys, time, value);
        if (!updated) InsertKey(ch->positionKeys, time, value);
    } else if (track == Track::Scale) {
        updated = UpdateKey(ch->scaleKeys, time, value);
        if (!updated) InsertKey(ch->scaleKeys, time, value);
    }
}

void KeyframeEditor::SetKeyframeValue(AnimationClip& clip, int boneIndex, float time, const math::Quat& value) {
    auto* ch = clip.FindOrCreateChannel(boneIndex);
    if (!ch) return;
    if (!UpdateKey(ch->rotationKeys, time, value)) {
        InsertKey(ch->rotationKeys, time, value);
    }
}

size_t KeyframeEditor::GetKeyframeCount(const AnimationClip& clip, int boneIndex) {
    const auto* ch = clip.FindChannel(boneIndex);
    if (!ch) return 0;
    return ch->positionKeys.size() + ch->rotationKeys.size() + ch->scaleKeys.size();
}

} // namespace animation
} // namespace eyelib
