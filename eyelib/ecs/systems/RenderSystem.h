#pragma once

#include "eyelib/ecs/System.h"

namespace eyelib {
namespace ecs {

class RenderSystem : public ISystem {
public:
    void Update(World& world, float deltaTime) override;
    std::string GetName() const override { return "RenderSystem"; }
    std::vector<std::string> GetDependencies() const override {
        return { "TransformSystem", "CameraSystem", "CullingSystem", "LightSystem" };
    }
    int GetUpdateOrder() const override { return 50; }
};

} // namespace ecs
} // namespace eyelib
