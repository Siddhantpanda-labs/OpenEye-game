#include "AnimatorController.h"

#include "core/log/Log.h"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace OpenEye {
namespace Animation {
namespace Controller {

static std::string ReadAllText(const std::string& path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static ParamType ParseParamType(const std::string& s) {
    if (s == "bool") return ParamType::Bool;
    if (s == "int") return ParamType::Int;
    if (s == "float") return ParamType::Float;
    if (s == "trigger") return ParamType::Trigger;
    return ParamType::Float;
}

static BlendTreeType ParseBlendTreeType(const std::string& s) {
    if (s == "1d") return BlendTreeType::Blend1D;
    if (s == "2d") return BlendTreeType::Blend2D;
    return BlendTreeType::Blend1D;
}

static MotionType ParseMotionType(const std::string& s) {
    if (s == "clip") return MotionType::Clip;
    if (s == "blendTree") return MotionType::BlendTree;
    return MotionType::Clip;
}

static ConditionOp ParseConditionOp(const std::string& s) {
    if (s == ">") return ConditionOp::Greater;
    if (s == ">=") return ConditionOp::GreaterEqual;
    if (s == "<") return ConditionOp::Less;
    if (s == "<=") return ConditionOp::LessEqual;
    if (s == "==") return ConditionOp::Equal;
    if (s == "!=") return ConditionOp::NotEqual;
    if (s == "true") return ConditionOp::IsTrue;
    if (s == "false") return ConditionOp::IsFalse;
    if (s == "triggered") return ConditionOp::Triggered;
    return ConditionOp::IsTrue;
}

std::shared_ptr<AnimatorController> AnimatorController::LoadFromFile(const std::string& path) {
    const std::string text = ReadAllText(path);
    if (text.empty()) {
        OE_WARN_F("AnimatorController: file not found or empty: %s", path.c_str());
        return nullptr;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(text);
    } catch (const std::exception& e) {
        OE_ERROR_F("AnimatorController: failed to parse JSON (%s): %s", path.c_str(), e.what());
        return nullptr;
    }

    auto ctrl = std::make_shared<AnimatorController>();
    ctrl->name = j.value("name", std::string("AnimatorController"));

    // Params
    if (j.contains("params") && j["params"].is_array()) {
        for (const auto& pj : j["params"]) {
            ParamDef def;
            def.name = pj.value("name", "");
            def.type = ParseParamType(pj.value("type", "float"));
            def.defaultBool = pj.value("defaultBool", false);
            def.defaultInt = pj.value("defaultInt", 0);
            def.defaultFloat = pj.value("defaultFloat", 0.0f);
            if (!def.name.empty()) ctrl->params.push_back(def);
        }
    }

    // Blend trees
    if (j.contains("blendTrees") && j["blendTrees"].is_array()) {
        for (const auto& btj : j["blendTrees"]) {
            BlendTree bt;
            bt.name = btj.value("name", "");
            bt.type = ParseBlendTreeType(btj.value("type", "1d"));
            bt.paramX = btj.value("paramX", "speed");
            bt.paramY = btj.value("paramY", "strafe");
            if (btj.contains("children") && btj["children"].is_array()) {
                for (const auto& cj : btj["children"]) {
                    BlendTreeChild c;
                    c.motionClip = cj.value("clip", "");
                    c.threshold = cj.value("threshold", 0.0f);
                    if (cj.contains("pos") && cj["pos"].is_array() && cj["pos"].size() >= 2) {
                        c.position = glm::vec2(cj[0].get<float>(), cj[1].get<float>());
                    } else if (cj.contains("position") && cj["position"].is_array() && cj["position"].size() >= 2) {
                        c.position = glm::vec2(cj["position"][0].get<float>(), cj["position"][1].get<float>());
                    } else if (cj.contains("x") && cj.contains("y")) {
                        c.position = glm::vec2(cj["x"].get<float>(), cj["y"].get<float>());
                    }
                    bt.children.push_back(c);
                }
            }
            if (!bt.name.empty()) ctrl->blendTrees[bt.name] = bt;
        }
    }

    // Bone masks
    if (j.contains("boneMasks") && j["boneMasks"].is_array()) {
        for (const auto& mj : j["boneMasks"]) {
            BoneMask m;
            m.name = mj.value("name", "");
            if (mj.contains("bones") && mj["bones"].is_array()) {
                for (const auto& b : mj["bones"]) {
                    m.includedBoneNames.push_back(b.get<std::string>());
                }
            }
            if (!m.name.empty()) ctrl->boneMasks[m.name] = m;
        }
    }

    // Events
    if (j.contains("events") && j["events"].is_array()) {
        for (const auto& ej : j["events"]) {
            const std::string clip = ej.value("clip", "");
            if (clip.empty()) continue;
            std::vector<AnimEvent> events;
            if (ej.contains("track") && ej["track"].is_array()) {
                for (const auto& evj : ej["track"]) {
                    AnimEvent ev;
                    ev.time = evj.value("time", 0.0f);
                    ev.name = evj.value("name", "");
                    ev.payload = evj.value("payload", "");
                    if (!ev.name.empty()) events.push_back(ev);
                }
            }
            ctrl->events.SetEvents(clip, std::move(events));
        }
    }

    // Layers
    if (j.contains("layers") && j["layers"].is_array()) {
        for (const auto& lj : j["layers"]) {
            Layer layer;
            layer.name = lj.value("name", "Base");
            layer.weight = lj.value("weight", 1.0f);
            layer.boneMask = lj.value("mask", "");
            layer.defaultState = lj.value("defaultState", "");

            // States
            if (lj.contains("states") && lj["states"].is_array()) {
                for (const auto& sj : lj["states"]) {
                    State st;
                    st.name = sj.value("name", "");
                    st.playbackSpeed = sj.value("speed", 1.0f);
                    st.loop = sj.value("loop", true);
                    if (sj.contains("motion")) {
                        const auto& mj = sj["motion"];
                        st.motion.type = ParseMotionType(mj.value("type", "clip"));
                        st.motion.clipName = mj.value("clip", "");
                        st.motion.blendTreeName = mj.value("blendTree", "");
                    } else {
                        st.motion.type = MotionType::Clip;
                        st.motion.clipName = sj.value("clip", "");
                    }
                    if (!st.name.empty()) layer.states[st.name] = st;
                }
            }

            // Transitions
            if (lj.contains("transitions") && lj["transitions"].is_array()) {
                for (const auto& tj : lj["transitions"]) {
                    Transition tr;
                    tr.fromState = tj.value("from", "");
                    tr.toState = tj.value("to", "");
                    tr.blendTime = tj.value("blend", 0.2f);
                    if (tj.contains("conditions") && tj["conditions"].is_array()) {
                        for (const auto& cj : tj["conditions"]) {
                            TransitionCondition c;
                            c.param = cj.value("param", "");
                            c.op = ParseConditionOp(cj.value("op", "true"));
                            c.threshold = cj.value("threshold", 0.0f);
                            c.thresholdInt = cj.value("thresholdInt", 0);
                            if (!c.param.empty()) tr.conditions.push_back(c);
                        }
                    }
                    if (!tr.fromState.empty() && !tr.toState.empty()) {
                        layer.transitions.push_back(tr);
                    }
                }
            }

            ctrl->layers.push_back(std::move(layer));
        }
    }

    OE_INFO_F("AnimatorController loaded: '%s' (params=%zu, layers=%zu, blendTrees=%zu, masks=%zu)",
              ctrl->name.c_str(), ctrl->params.size(), ctrl->layers.size(),
              ctrl->blendTrees.size(), ctrl->boneMasks.size());

    return ctrl;
}

} // namespace Controller
} // namespace Animation
} // namespace OpenEye

