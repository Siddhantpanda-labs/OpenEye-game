#include "Serialization.h"

namespace eyelib {
namespace api {
namespace world {

bool Serializer::Save(const ::eyelib::world::WorldState& state, const std::string& path) const {
    return m_serializer.Save(state, path);
}

bool Serializer::Load(::eyelib::world::WorldState& state, const std::string& path) const {
    return m_serializer.Load(state, path);
}

} // namespace world
} // namespace api
} // namespace eyelib
