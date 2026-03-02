#pragma once

#include "eyelib/math/Types.h"
#include "eyelib/world/EntityId.h"
#include <string>
#include <variant>

namespace eyelib {
namespace scripting {

/**
 * @brief Represents a value that can be passed between C++ and Lua.
 */
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
    
    std::variant<
        std::monostate,      // Nil
        bool,                // Bool
        double,              // Number
        std::string,         // String
        math::Vec3,          // Vec3
        world::EntityId      // Entity
    > value;

    ScriptValue() : type(Type::Nil) {}
    ScriptValue(bool b) : type(Type::Bool), value(b) {}
    ScriptValue(double n) : type(Type::Number), value(n) {}
    ScriptValue(float n) : type(Type::Number), value(static_cast<double>(n)) {}
    ScriptValue(int n) : type(Type::Number), value(static_cast<double>(n)) {}
    ScriptValue(const std::string& s) : type(Type::String), value(s) {}
    ScriptValue(const char* s) : type(Type::String), value(std::string(s)) {}
    ScriptValue(const math::Vec3& v) : type(Type::Vec3), value(v) {}
    ScriptValue(world::EntityId e) : type(Type::Entity), value(e) {}

    bool IsNil() const { return type == Type::Nil; }
    bool IsBool() const { return type == Type::Bool; }
    bool IsNumber() const { return type == Type::Number; }
    bool IsString() const { return type == Type::String; }
    bool IsVec3() const { return type == Type::Vec3; }
    bool IsEntity() const { return type == Type::Entity; }

    bool GetBool() const {
        if (type == Type::Bool) return std::get<bool>(value);
        return false;
    }

    double GetNumber() const {
        if (type == Type::Number) return std::get<double>(value);
        return 0.0;
    }

    std::string GetString() const {
        if (type == Type::String) return std::get<std::string>(value);
        return "";
    }

    math::Vec3 GetVec3() const {
        if (type == Type::Vec3) return std::get<math::Vec3>(value);
        return math::Vec3(0.0f);
    }

    world::EntityId GetEntity() const {
        if (type == Type::Entity) return std::get<world::EntityId>(value);
        return world::InvalidEntity;
    }
};

} // namespace scripting
} // namespace eyelib
