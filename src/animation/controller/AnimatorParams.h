#pragma once

#include <string>
#include <unordered_map>
#include <variant>

namespace OpenEye {
namespace Animation {
namespace Controller {

enum class ParamType {
    Bool,
    Int,
    Float,
    Trigger
};

struct ParamDef {
    std::string name;
    ParamType type = ParamType::Float;
    bool defaultBool = false;
    int defaultInt = 0;
    float defaultFloat = 0.0f;
};

// Runtime parameter storage (instance-level)
class AnimatorParams {
public:
    void Define(const ParamDef& def);

    bool Has(const std::string& name) const;
    ParamType GetType(const std::string& name) const;

    // Setters
    void SetBool(const std::string& name, bool v);
    void SetInt(const std::string& name, int v);
    void SetFloat(const std::string& name, float v);
    void SetTrigger(const std::string& name);

    // Getters (returns default values if missing)
    bool GetBool(const std::string& name) const;
    int GetInt(const std::string& name) const;
    float GetFloat(const std::string& name) const;

    // Trigger is consumed on read by transitions
    bool ConsumeTrigger(const std::string& name);

private:
    struct Value {
        ParamType type = ParamType::Float;
        std::variant<bool, int, float> v = 0.0f;
        bool triggerSet = false;
    };

    std::unordered_map<std::string, Value> m_values;
};

} // namespace Controller
} // namespace Animation
} // namespace OpenEye

