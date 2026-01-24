#pragma once

#include "eyelib/ecs/System.h"

namespace eyelib {
namespace ecs {

class AnimationSystem : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    std::string GetName() const override { return "AnimationSystem"; }
    std::vector<std::string> GetDependencies() const override { return { "TransformSystem" }; }
    int GetUpdateOrder() const override { return 10; }
};

} // namespace ecs
} // namespace eyelib
