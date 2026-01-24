#pragma once

#include "eyelib/ecs/System.h"

namespace eyelib {
namespace ecs {

class TransformSystem : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    std::string GetName() const override { return "TransformSystem"; }
    int GetUpdateOrder() const override { return 0; }
};

} // namespace ecs
} // namespace eyelib
