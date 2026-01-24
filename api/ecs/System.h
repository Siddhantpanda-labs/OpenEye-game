#pragma once

#include "World.h"

#include "eyelib/ecs/SystemManager.h"

#include <memory>

namespace eyelib {
namespace api {

class SystemManager {
public:
    SystemManager();
    ~SystemManager();

    void RegisterSystem(std::shared_ptr<eyelib::ecs::ISystem> system);
    void UpdateAll(World& world, float deltaTime);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace api
} // namespace eyelib
