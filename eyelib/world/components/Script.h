#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/world/EntityId.h"

#include <string>
#include <unordered_map>
#include <variant>

namespace eyelib {
namespace world {
namespace components {

struct ScriptValue {
    enum class Type {
        Nil,
        Bool,
        Number,
        String,
        Vec3,
        Entity
    };

    Type type = Type::Nil;
    std::variant<std::monostate, bool, double, std::string, math::Vec3, EntityId> value;

    ScriptValue() = default;
    ScriptValue(bool v) : type(Type::Bool), value(v) {}
    ScriptValue(double v) : type(Type::Number), value(v) {}
    ScriptValue(float v) : type(Type::Number), value(static_cast<double>(v)) {}
    ScriptValue(int v) : type(Type::Number), value(static_cast<double>(v)) {}
    ScriptValue(const std::string& v) : type(Type::String), value(v) {}
    ScriptValue(const char* v) : type(Type::String), value(std::string(v)) {}
    ScriptValue(const math::Vec3& v) : type(Type::Vec3), value(v) {}
    ScriptValue(EntityId v) : type(Type::Entity), value(v) {}
};

struct Script {
    std::string scriptName;
    bool isEnabled = true;
    bool needsReload = false;
    std::unordered_map<std::string, ScriptValue> properties;
};

} // namespace components
} // namespace world
} // namespace eyelib
