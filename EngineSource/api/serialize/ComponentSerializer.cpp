#include "ComponentSerializer.h"

namespace eyelib {
namespace api {
namespace serialize {

std::string ComponentSerializer::SerializeComponent(uint32_t /*typeId*/, const void* /*component*/) {
    return "{}";
}

void* ComponentSerializer::DeserializeComponent(uint32_t /*typeId*/, const std::string& /*jsonText*/) {
    return nullptr;
}

} // namespace serialize
} // namespace api
} // namespace eyelib
