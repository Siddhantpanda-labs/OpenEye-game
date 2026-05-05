#pragma once

#include <cstdint>
#include <string>

namespace eyelib {
namespace api {
namespace serialize {

class ComponentSerializer {
public:
    static std::string SerializeComponent(uint32_t typeId, const void* component);
    static void* DeserializeComponent(uint32_t typeId, const std::string& jsonText);
};

} // namespace serialize
} // namespace api
} // namespace eyelib
