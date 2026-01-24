#pragma once

#include "eyelib/ecs/System.h"

namespace eyelib {
namespace ecs {

class CameraSystem : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    std::string GetName() const override { return "CameraSystem"; }
    std::vector<std::string> GetDependencies() const override { return { "TransformSystem" }; }
    int GetUpdateOrder() const override { return 30; }
};

} // namespace ecs
} // namespace eyelib
