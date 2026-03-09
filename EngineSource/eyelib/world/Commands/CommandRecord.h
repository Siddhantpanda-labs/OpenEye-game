#pragma once

#include "Command.h"
#include "eyelib/world/EntityId.h"

#include <json.hpp>

#include <memory>
#include <optional>
#include <string>

namespace eyelib {
namespace world {

struct CommandRecord {
    std::string type;
    nlohmann::json payload;
};

class CommandCodec {
public:
    static std::optional<CommandRecord> Encode(const Command& command);
    static std::unique_ptr<Command> Decode(const CommandRecord& record);
};

} // namespace world
} // namespace eyelib
