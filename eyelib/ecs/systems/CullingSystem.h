#pragma once

#include "eyelib/ecs/System.h"

namespace eyelib {
namespace ecs {

class CullingSystem : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    std::string GetName() const override { return "CullingSystem"; }
    std::vector<std::string> GetDependencies() const override { return { "CameraSystem", "TransformSystem" }; }
    int GetUpdateOrder() const override { return 40; }
};

} // namespace ecs
} // namespace eyelib
