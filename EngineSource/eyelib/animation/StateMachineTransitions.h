#pragma once

#include "StateMachineConditions.h"
#include <memory>
#include <vector>

namespace eyelib {
namespace animation {

enum class BlendMode {
    Linear
};

struct Transition {
    int fromState = -1;
    int toState = -1;
    float duration = 0.2f;
    BlendMode blendMode = BlendMode::Linear;
    std::vector<std::shared_ptr<Condition>> conditions;
};

} // namespace animation
} // namespace eyelib
