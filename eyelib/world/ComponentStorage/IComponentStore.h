#pragma once

#include "eyelib/world/EntityId.h"

namespace eyelib {
namespace world {

class IComponentStore {
public:
    virtual ~IComponentStore() = default;

    virtual void Remove(EntityId entity) = 0;
    virtual bool Has(EntityId entity) const = 0;
    virtual void Clear() = 0;
};

} // namespace world
} // namespace eyelib
