#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include "AnimatorParams.h"
#include "BlendTree.h"
#include "BoneMask.h"
#include "AnimEventTrack.h"

namespace OpenEye {
namespace Animation {
namespace Controller {

enum class ConditionOp {
    // Float/int comparisons
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Equal,
    NotEqual,
    // Bool
    IsTrue,
    IsFalse,
    // Trigger
    Triggered
};

struct TransitionCondition {
    std::string param;
    ConditionOp op = ConditionOp::IsTrue;
    float threshold = 0.0f; // used for numeric comparisons
    int thresholdInt = 0;   // used for int comparisons
};

struct Transition {
    std::string fromState;
    std::string toState;
    float blendTime = 0.2f;
    std::vector<TransitionCondition> conditions;
};

enum class MotionType {
    Clip,
    BlendTree
};

struct Motion {
    MotionType type = MotionType::Clip;
    std::string clipName;      // if Clip
    std::string blendTreeName; // if BlendTree
};

struct State {
    std::string name;
    Motion motion;
    float playbackSpeed = 1.0f;
    bool loop = true;
};

struct Layer {
    std::string name = "Base";
    float weight = 1.0f;
    std::string boneMask;          // optional mask name
    std::string defaultState;
    std::unordered_map<std::string, State> states;
    std::vector<Transition> transitions;
};

class AnimatorController {
public:
    std::string name;

    std::vector<ParamDef> params;
    std::unordered_map<std::string, BlendTree> blendTrees;
    std::unordered_map<std::string, BoneMask> boneMasks;
    AnimEventTrack events;
    std::vector<Layer> layers;

    // Load from JSON file on disk (controller asset)
    static std::shared_ptr<AnimatorController> LoadFromFile(const std::string& path);
};

} // namespace Controller
} // namespace Animation
} // namespace OpenEye

