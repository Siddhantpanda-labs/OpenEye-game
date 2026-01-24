#pragma once

#include <string>
#include <vector>

namespace eyelib {
namespace ecs {

class World;

class ISystem {
public:
    virtual ~ISystem() = default;

    virtual void Update(World& world, float deltaTime) = 0;
    virtual std::string GetName() const = 0;
    virtual std::vector<std::string> GetDependencies() const { return {}; }
    virtual int GetUpdateOrder() const { return 0; }
};

} // namespace ecs
} // namespace eyelib
