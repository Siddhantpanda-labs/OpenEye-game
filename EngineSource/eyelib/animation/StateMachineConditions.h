#pragma once

#include <memory>
#include <string>

namespace eyelib {
namespace animation {

class AnimationStateMachine;

enum class ConditionOp {
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    Equal,
    NotEqual
};

class Condition {
public:
    virtual ~Condition() = default;
    virtual bool Evaluate(const AnimationStateMachine& sm) const = 0;
};

class BoolCondition : public Condition {
public:
    explicit BoolCondition(const std::string& name, bool value = true)
        : m_name(name), m_value(value) {}
    bool Evaluate(const AnimationStateMachine& sm) const override;

private:
    std::string m_name;
    bool m_value = true;
};

class FloatCondition : public Condition {
public:
    FloatCondition(const std::string& name, ConditionOp op, float threshold)
        : m_name(name), m_op(op), m_threshold(threshold) {}
    bool Evaluate(const AnimationStateMachine& sm) const override;

private:
    std::string m_name;
    ConditionOp m_op = ConditionOp::Greater;
    float m_threshold = 0.0f;
};

class TriggerCondition : public Condition {
public:
    explicit TriggerCondition(const std::string& name) : m_name(name) {}
    bool Evaluate(const AnimationStateMachine& sm) const override;
    const std::string& Name() const { return m_name; }

private:
    std::string m_name;
};

} // namespace animation
} // namespace eyelib
