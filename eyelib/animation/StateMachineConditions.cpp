#include "StateMachineConditions.h"
#include "AnimationStateMachine.h"

namespace eyelib {
namespace animation {

bool BoolCondition::Evaluate(const AnimationStateMachine& sm) const {
    return sm.GetCondition(m_name) == m_value;
}

bool FloatCondition::Evaluate(const AnimationStateMachine& sm) const {
    float v = sm.GetConditionFloat(m_name);
    switch (m_op) {
        case ConditionOp::Greater: return v > m_threshold;
        case ConditionOp::GreaterEqual: return v >= m_threshold;
        case ConditionOp::Less: return v < m_threshold;
        case ConditionOp::LessEqual: return v <= m_threshold;
        case ConditionOp::Equal: return v == m_threshold;
        case ConditionOp::NotEqual: return v != m_threshold;
        default: return false;
    }
}

bool TriggerCondition::Evaluate(const AnimationStateMachine& sm) const {
    return sm.GetTrigger(m_name);
}

} // namespace animation
} // namespace eyelib
