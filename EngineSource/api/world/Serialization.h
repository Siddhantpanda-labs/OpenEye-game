#pragma once

#include "eyelib/world/Serialization/WorldSerializer.h"

namespace eyelib {
namespace api {
namespace world {

class Serializer {
public:
    bool Save(const ::eyelib::world::WorldState& state, const std::string& path) const;
    bool Load(::eyelib::world::WorldState& state, const std::string& path) const;

private:
    ::eyelib::world::WorldSerializer m_serializer;
};

} // namespace world
} // namespace api
} // namespace eyelib
