#include "AnimatorSystem.h"
#include "../Components.h"

#include "animation/AnimationClip.h"
#include "animation/Skeleton.h"
#include "animation/AnimationSystem.h"
#include "animation/controller/AnimatorController.h"
#include "animation/controller/BoneMask.h"
#include "animation/controller/BlendTree.h"

#include "core/log/Log.h"
#include "physics/PhysicsWorld.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace OpenEye {
namespace ECS {

using namespace OpenEye::Animation;
using namespace OpenEye::Animation::Controller;

static inline void DecomposeTRS(const glm::mat4& m, glm::vec3& outT, glm::quat& outR, glm::vec3& outS) {
    outT = glm::vec3(m[3]);
    outS = glm::vec3(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
    glm::mat3 rotMat(
        glm::vec3(m[0]) / (outS.x > 1e-6f ? outS.x : 1.0f),
        glm::vec3(m[1]) / (outS.y > 1e-6f ? outS.y : 1.0f),
        glm::vec3(m[2]) / (outS.z > 1e-6f ? outS.z : 1.0f)
    );
    outR = glm::quat_cast(rotMat);
}

static inline glm::mat4 ComposeTRS(const glm::vec3& t, const glm::quat& r, const glm::vec3& s) {
    glm::mat4 out = glm::translate(glm::mat4(1.0f), t);
    out = out * glm::mat4_cast(r);
    out = glm::scale(out, s);
    return out;
}

static inline glm::mat4 BlendMat4(const glm::mat4& a, const glm::mat4& b, float t) {
    t = glm::clamp(t, 0.0f, 1.0f);
    glm::vec3 ta, tb, sa, sb;
    glm::quat ra, rb;
    DecomposeTRS(a, ta, ra, sa);
    DecomposeTRS(b, tb, rb, sb);
    glm::vec3 tOut = glm::mix(ta, tb, t);
    glm::vec3 sOut = glm::mix(sa, sb, t);
    glm::quat rOut = glm::slerp(ra, rb, t);
    return ComposeTRS(tOut, rOut, sOut);
}

static int FindClipIndexByName(const ECS::AnimationComponent& anim, const std::string& clipName) {
    for (int i = 0; i < (int)anim.clips.size(); ++i) {
        auto clip = std::static_pointer_cast<AnimationClip>(anim.clips[size_t(i)]);
        if (clip && clip->GetName() == clipName) return i;
    }
    return -1;
}

static void SampleClipLocal(
    const ECS::AnimationComponent& anim,
    int clipIndex,
    float time,
    std::vector<glm::mat4>& outLocal
) {
    if (!anim.skeleton) return;
    auto skeleton = std::static_pointer_cast<Skeleton>(anim.skeleton);
    if (!skeleton) return;
    const size_t boneCount = skeleton->GetBoneCount();
    if (boneCount == 0) return;

    // init bind pose
    for (size_t i = 0; i < boneCount; ++i) outLocal[i] = skeleton->GetBone(i).GetBindLocalMatrix();
    if (clipIndex < 0 || clipIndex >= (int)anim.clips.size()) return;
    auto clip = std::static_pointer_cast<AnimationClip>(anim.clips[size_t(clipIndex)]);
    if (!clip) return;

    std::vector<int> boneIndices(boneCount);
    for (size_t i = 0; i < boneCount; ++i) boneIndices[i] = (int)i;
    clip->Sample(time, outLocal, boneIndices);
}

static bool EvalCondition(const TransitionCondition& c, AnimatorParams& params) {
    const ParamType t = params.GetType(c.param);
    switch (c.op) {
        case ConditionOp::Triggered:
            return (t == ParamType::Trigger) ? params.ConsumeTrigger(c.param) : false;
        case ConditionOp::IsTrue:
            return (t == ParamType::Bool) ? params.GetBool(c.param) : false;
        case ConditionOp::IsFalse:
            return (t == ParamType::Bool) ? !params.GetBool(c.param) : false;
        case ConditionOp::Greater:
            if (t == ParamType::Float) return params.GetFloat(c.param) > c.threshold;
            if (t == ParamType::Int) return params.GetInt(c.param) > c.thresholdInt;
            return false;
        case ConditionOp::GreaterEqual:
            if (t == ParamType::Float) return params.GetFloat(c.param) >= c.threshold;
            if (t == ParamType::Int) return params.GetInt(c.param) >= c.thresholdInt;
            return false;
        case ConditionOp::Less:
            if (t == ParamType::Float) return params.GetFloat(c.param) < c.threshold;
            if (t == ParamType::Int) return params.GetInt(c.param) < c.thresholdInt;
            return false;
        case ConditionOp::LessEqual:
            if (t == ParamType::Float) return params.GetFloat(c.param) <= c.threshold;
            if (t == ParamType::Int) return params.GetInt(c.param) <= c.thresholdInt;
            return false;
        case ConditionOp::Equal:
            if (t == ParamType::Float) return std::abs(params.GetFloat(c.param) - c.threshold) < 1e-4f;
            if (t == ParamType::Int) return params.GetInt(c.param) == c.thresholdInt;
            return false;
        case ConditionOp::NotEqual:
            if (t == ParamType::Float) return std::abs(params.GetFloat(c.param) - c.threshold) >= 1e-4f;
            if (t == ParamType::Int) return params.GetInt(c.param) != c.thresholdInt;
            return false;
    }
    return false;
}

static bool TransitionAllowed(const Transition& tr, AnimatorParams& params) {
    for (const auto& c : tr.conditions) {
        if (!EvalCondition(c, params)) return false;
    }
    return true;
}

static void PushEvent(AnimatorComponent& animator, const std::string& clip, const AnimEvent& ev) {
    AnimatorComponent::FiredEvent fe;
    fe.clip = clip;
    fe.name = ev.name;
    fe.payload = ev.payload;
    fe.time = ev.time;
    animator.recentEvents.push_back(std::move(fe));
    if ((int)animator.recentEvents.size() > animator.maxRecentEvents) {
        animator.recentEvents.erase(animator.recentEvents.begin(),
                                    animator.recentEvents.begin() + (animator.recentEvents.size() - animator.maxRecentEvents));
    }
}

static void FireEventsForClip(
    AnimatorComponent& animator,
    const AnimatorController& ctrl,
    const std::string& clipName,
    float prevTime,
    float currTime,
    float duration,
    bool loop
) {
    const auto* track = ctrl.events.GetEvents(clipName);
    if (!track || track->empty()) return;

    auto scan = [&](float a, float b) {
        for (const auto& ev : *track) {
            if (ev.time > a && ev.time <= b) {
                PushEvent(animator, clipName, ev);
            }
        }
    };

    if (!loop || duration <= 0.0f) {
        if (currTime > prevTime) scan(prevTime, currTime);
        return;
    }

    // Handle wrap-around
    if (currTime >= prevTime) {
        scan(prevTime, currTime);
    } else {
        scan(prevTime, duration);
        scan(0.0f, currTime);
    }
}

void AnimatorSystem::Update(Scene& scene, float deltaTime, void* physicsWorldVoid) {
    auto& registry = scene.GetRegistry();
    auto view = registry.view<AnimatorComponent, AnimationComponent, Transform>();

    Physics::PhysicsWorld* physicsWorld = static_cast<Physics::PhysicsWorld*>(physicsWorldVoid);

    for (auto e : view) {
        auto& animator = view.get<AnimatorComponent>(e);
        auto& anim = view.get<AnimationComponent>(e);
        auto& transform = view.get<Transform>(e);

        if (!anim.skeleton) continue;
        auto skeleton = std::static_pointer_cast<Skeleton>(anim.skeleton);
        if (!skeleton) continue;
        const size_t boneCount = skeleton->GetBoneCount();
        if (boneCount == 0) continue;

        // Load controller if needed
        if (!animator.controller && !animator.controllerPath.empty()) {
            auto ctrl = AnimatorController::LoadFromFile(animator.controllerPath);
            animator.controller = ctrl;
            if (ctrl) {
                animator.params = AnimatorParams{};
                for (const auto& p : ctrl->params) animator.params.Define(p);
                animator.layers.clear();
                animator.layers.resize(ctrl->layers.size());
                for (size_t li = 0; li < ctrl->layers.size(); ++li) {
                    animator.layers[li].currentState = ctrl->layers[li].defaultState;
                }
            }
        }

        auto ctrl = std::static_pointer_cast<AnimatorController>(animator.controller);
        if (!ctrl) continue;
        if (ctrl->layers.empty()) continue;

        // Allocate per-layer pose buffers
        std::vector<glm::mat4> finalLocal(boneCount, glm::mat4(1.0f));
        bool finalInitialized = false;

        // Root motion (applied from base layer only)
        glm::vec3 rootDeltaPos(0.0f);
        glm::quat rootDeltaRot(1.0f, 0.0f, 0.0f, 0.0f);

        for (size_t li = 0; li < ctrl->layers.size(); ++li) {
            const auto& layer = ctrl->layers[li];
            if (li >= animator.layers.size()) break;
            auto& lr = animator.layers[li];

            // Ensure current state
            if (lr.currentState.empty()) lr.currentState = layer.defaultState;
            if (layer.states.find(lr.currentState) == layer.states.end()) {
                // fallback: first state
                if (!layer.states.empty()) lr.currentState = layer.states.begin()->first;
            }

            // Evaluate transitions (from current state only)
            for (const auto& tr : layer.transitions) {
                if (tr.fromState != lr.currentState) continue;
                if (!TransitionAllowed(tr, animator.params)) continue;
                // Start transition
                lr.inTransition = true;
                lr.blendTime = 0.0f;
                lr.blendDuration = std::max(0.0f, tr.blendTime);
                lr.fromState = lr.currentState;
                lr.fromTime = lr.stateTime;
                lr.previousState = lr.currentState;
                lr.currentState = tr.toState;
                lr.stateTime = 0.0f;
                break;
            }

            const auto itState = layer.states.find(lr.currentState);
            if (itState == layer.states.end()) continue;
            const State& state = itState->second;

            // Advance time
            const float prevTime = lr.stateTime;
            lr.stateTime += deltaTime * state.playbackSpeed;

            // Determine clip(s) to sample
            std::vector<glm::mat4> localPose(boneCount, glm::mat4(1.0f));
            std::vector<glm::mat4> localFrom(boneCount, glm::mat4(1.0f));

            std::string dominantClipName;
            float duration = 0.0f;
            bool loop = state.loop;

            auto sampleMotion = [&](const Motion& m, float time, std::vector<glm::mat4>& out) {
                if (m.type == MotionType::Clip) {
                    const int idx = FindClipIndexByName(anim, m.clipName);
                    SampleClipLocal(anim, idx, time, out);
                } else {
                    auto btIt = ctrl->blendTrees.find(m.blendTreeName);
                    if (btIt == ctrl->blendTrees.end()) return;
                    const BlendTree& bt = btIt->second;
                    std::vector<float> weights;
                    if (bt.type == BlendTreeType::Blend1D) {
                        weights = bt.Evaluate1D(animator.params.GetFloat(bt.paramX));
                    } else {
                        weights = bt.Evaluate2D(glm::vec2(animator.params.GetFloat(bt.paramX),
                                                          animator.params.GetFloat(bt.paramY)));
                    }
                    // Weighted blend of child clips
                    std::vector<glm::mat4> tmp(boneCount, glm::mat4(1.0f));
                    bool first = true;
                    float bestW = -1.0f;
                    for (size_t ci = 0; ci < bt.children.size() && ci < weights.size(); ++ci) {
                        const float w = weights[ci];
                        if (w <= 0.0f) continue;
                        const int idx = FindClipIndexByName(anim, bt.children[ci].motionClip);
                        tmp.assign(boneCount, glm::mat4(1.0f));
                        SampleClipLocal(anim, idx, time, tmp);
                        if (first) { out = tmp; first = false; }
                        else {
                            // incremental blend (approx)
                            for (size_t bi = 0; bi < boneCount; ++bi) {
                                out[bi] = BlendMat4(out[bi], tmp[bi], w);
                            }
                        }
                        if (w > bestW) {
                            bestW = w;
                            dominantClipName = bt.children[ci].motionClip;
                        }
                    }
                }
            };

            // For duration/event/time wrap, use dominant clip
            auto resolveDuration = [&](const std::string& clipName) {
                const int idx = FindClipIndexByName(anim, clipName);
                auto clip = (idx >= 0) ? std::static_pointer_cast<AnimationClip>(anim.clips[size_t(idx)]) : nullptr;
                duration = clip ? clip->GetDuration() : 0.0f;
            };

            // Sample TO pose
            sampleMotion(state.motion, lr.stateTime, localPose);
            if (state.motion.type == MotionType::Clip) dominantClipName = state.motion.clipName;
            resolveDuration(dominantClipName);

            // Handle looping / clamp time
            if (duration > 0.0f) {
                if (loop) {
                    while (lr.stateTime >= duration) lr.stateTime -= duration;
                } else {
                    lr.stateTime = std::min(lr.stateTime, duration);
                }
            }

            // Sample FROM pose if transitioning
            float blendFactor = 1.0f;
            if (lr.inTransition && !lr.fromState.empty()) {
                lr.blendTime += deltaTime;
                blendFactor = (lr.blendDuration > 0.0f) ? std::min(1.0f, lr.blendTime / lr.blendDuration) : 1.0f;

                auto fromIt = layer.states.find(lr.fromState);
                if (fromIt != layer.states.end()) {
                    const State& fromState = fromIt->second;
                    dominantClipName = (fromState.motion.type == MotionType::Clip) ? fromState.motion.clipName : dominantClipName;
                    sampleMotion(fromState.motion, lr.fromTime, localFrom);
                }

                for (size_t bi = 0; bi < boneCount; ++bi) {
                    localPose[bi] = BlendMat4(localFrom[bi], localPose[bi], blendFactor);
                }

                if (blendFactor >= 1.0f) {
                    lr.inTransition = false;
                    lr.fromState.clear();
                }
            }

            // Root motion from base layer: compute delta from root bone local transform
            if (li == 0 && animator.applyRootMotion && duration > 0.0f) {
                const int root = skeleton->GetRootBoneIndex();
                if (root >= 0) {
                    // Sample root at prev and curr times on dominant clip only (simple)
                    const int idx = FindClipIndexByName(anim, dominantClipName);
                    std::vector<glm::mat4> tmpPrev(boneCount, glm::mat4(1.0f));
                    std::vector<glm::mat4> tmpCurr(boneCount, glm::mat4(1.0f));
                    SampleClipLocal(anim, idx, prevTime, tmpPrev);
                    SampleClipLocal(anim, idx, lr.stateTime, tmpCurr);
                    glm::vec3 t0, t1, s0, s1;
                    glm::quat r0, r1;
                    DecomposeTRS(tmpPrev[size_t(root)], t0, r0, s0);
                    DecomposeTRS(tmpCurr[size_t(root)], t1, r1, s1);
                    rootDeltaPos = (t1 - t0);
                    rootDeltaRot = r1 * glm::inverse(r0);
                }
            }

            // Fire events (dominant clip only)
            if (!dominantClipName.empty() && duration > 0.0f) {
                FireEventsForClip(animator, *ctrl, dominantClipName, prevTime, lr.stateTime, duration, loop);
            }

            // Layer blend into finalLocal
            float layerW = glm::clamp(layer.weight, 0.0f, 1.0f);
            if (!finalInitialized) {
                finalLocal = localPose;
                finalInitialized = true;
            } else if (layerW > 0.0f) {
                // Optional mask
                std::vector<float> maskWeights;
                if (!layer.boneMask.empty()) {
                    auto maskIt = ctrl->boneMasks.find(layer.boneMask);
                    if (maskIt != ctrl->boneMasks.end()) {
                        auto compiled = BoneMaskCompiler::Compile(maskIt->second, *skeleton);
                        maskWeights = std::move(compiled.weights);
                    }
                }
                for (size_t bi = 0; bi < boneCount; ++bi) {
                    const float mw = maskWeights.empty() ? 1.0f : maskWeights[bi];
                    const float t = layerW * mw;
                    if (t <= 0.0f) continue;
                    finalLocal[bi] = BlendMat4(finalLocal[bi], localPose[bi], t);
                }
            }
        }

        if (!finalInitialized) continue;

        // Build final bone matrices for skinning (reuse existing AnimationSystem helper)
        if (anim.finalBoneMatrices.size() != boneCount) {
            anim.finalBoneMatrices.resize(boneCount, glm::mat4(1.0f));
        }
        AnimationSystem::BuildBoneMatrices(*skeleton, finalLocal, anim.finalBoneMatrices);
        anim.boneMatricesDirty = false;

        // Apply root motion
        if (animator.applyRootMotion) {
            transform.position += rootDeltaPos;
            transform.rotation = glm::normalize(rootDeltaRot * transform.rotation);
            transform.dirty = true;

            if (animator.rootMotionAffectsPhysics && physicsWorld && registry.all_of<PhysicsBody>(e)) {
                auto& pb = registry.get<PhysicsBody>(e);
                if (pb.rigidBody && pb.isKinematic) {
                    btTransform btT = pb.rigidBody->getWorldTransform();
                    btVector3 p(btT.getOrigin().x() + rootDeltaPos.x,
                                btT.getOrigin().y() + rootDeltaPos.y,
                                btT.getOrigin().z() + rootDeltaPos.z);
                    btT.setOrigin(p);
                    pb.rigidBody->setWorldTransform(btT);
                }
            }
        }
    }
}

} // namespace ECS
} // namespace OpenEye

