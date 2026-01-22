#include "AnimatorParams.h"
#include "core/log/Log.h"

namespace OpenEye {
namespace Animation {
namespace Controller {

void AnimatorParams::Define(const ParamDef& def) {
    if (def.name.empty()) return;

    Value val;
    val.type = def.type;
    switch (def.type) {
        case ParamType::Bool: val.v = def.defaultBool; break;
        case ParamType::Int: val.v = def.defaultInt; break;
        case ParamType::Float: val.v = def.defaultFloat; break;
        case ParamType::Trigger:
            val.v = false;
            val.triggerSet = false;
            break;
    }
    m_values[def.name] = val;
}

bool AnimatorParams::Has(const std::string& name) const {
    return m_values.find(name) != m_values.end();
}

ParamType AnimatorParams::GetType(const std::string& name) const {
    auto it = m_values.find(name);
    if (it == m_values.end()) return ParamType::Float;
    return it->second.type;
}

void AnimatorParams::SetBool(const std::string& name, bool v) {
    auto it = m_values.find(name);
    if (it == m_values.end()) return;
    if (it->second.type != ParamType::Bool) return;
    it->second.v = v;
}

void AnimatorParams::SetInt(const std::string& name, int v) {
    auto it = m_values.find(name);
    if (it == m_values.end()) return;
    if (it->second.type != ParamType::Int) return;
    it->second.v = v;
}

void AnimatorParams::SetFloat(const std::string& name, float v) {
    auto it = m_values.find(name);
    if (it == m_values.end()) return;
    if (it->second.type != ParamType::Float) return;
    it->second.v = v;
}

void AnimatorParams::SetTrigger(const std::string& name) {
    auto it = m_values.find(name);
    if (it == m_values.end()) return;
    if (it->second.type != ParamType::Trigger) return;
    it->second.triggerSet = true;
}

bool AnimatorParams::GetBool(const std::string& name) const {
    auto it = m_values.find(name);
    if (it == m_values.end()) return false;
    if (it->second.type != ParamType::Bool) return false;
    return std::get<bool>(it->second.v);
}

int AnimatorParams::GetInt(const std::string& name) const {
    auto it = m_values.find(name);
    if (it == m_values.end()) return 0;
    if (it->second.type != ParamType::Int) return 0;
    return std::get<int>(it->second.v);
}

float AnimatorParams::GetFloat(const std::string& name) const {
    auto it = m_values.find(name);
    if (it == m_values.end()) return 0.0f;
    if (it->second.type != ParamType::Float) return 0.0f;
    return std::get<float>(it->second.v);
}

bool AnimatorParams::ConsumeTrigger(const std::string& name) {
    auto it = m_values.find(name);
    if (it == m_values.end()) return false;
    if (it->second.type != ParamType::Trigger) return false;
    const bool wasSet = it->second.triggerSet;
    it->second.triggerSet = false;
    return wasSet;
}

} // namespace Controller
} // namespace Animation
} // namespace OpenEye

