#include "ComponentSerializer.h"

#include "eyelib/serialize/ComponentSerializer.h"

#include <json.hpp>

namespace eyelib {
namespace api {
namespace serialize {

std::string ComponentSerializer::SerializeComponent(uint32_t typeId, const void* component) {
    auto json = eyelib::serialize::ComponentSerializer::Get().SerializeComponent(typeId, component);
    return json.dump();
}

void* ComponentSerializer::DeserializeComponent(uint32_t typeId, const std::string& jsonText) {
    try {
        const auto json = nlohmann::json::parse(jsonText);
        return eyelib::serialize::ComponentSerializer::Get().DeserializeComponent(typeId, json);
    } catch (...) {
        return nullptr;
    }
}

} // namespace serialize
} // namespace api
} // namespace eyelib
