#include "WorldSnapshot.h"

#include "WorldSerializer.h"

namespace eyelib {
namespace world {

WorldSnapshot::WorldSnapshot(nlohmann::json data)
    : m_data(std::move(data)) {}

WorldSnapshot WorldSnapshot::Capture(const WorldState& state, const WorldSerializer& serializer) {
    return WorldSnapshot(serializer.Serialize(state));
}

bool WorldSnapshot::Restore(WorldState& state, const WorldSerializer& serializer) const {
    return serializer.Deserialize(state, m_data);
}

const nlohmann::json& WorldSnapshot::GetData() const {
    return m_data;
}

} // namespace world
} // namespace eyelib
