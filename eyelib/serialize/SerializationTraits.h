#pragma once

#include "eyelib/math/Transform.h"
#include "eyelib/math/Types.h"

#include <json.hpp>
#include <string>
#include <type_traits>
#include <vector>

namespace eyelib {
namespace serialize {

template<typename T, typename Enable = void>
struct SerializationTraits;

template<>
struct SerializationTraits<int> {
    static nlohmann::json Serialize(int v) { return v; }
    static int Deserialize(const nlohmann::json& j, int fallback = 0) { return j.is_number_integer() ? j.get<int>() : fallback; }
};

template<>
struct SerializationTraits<float> {
    static nlohmann::json Serialize(float v) { return v; }
    static float Deserialize(const nlohmann::json& j, float fallback = 0.0f) { return j.is_number() ? j.get<float>() : fallback; }
};

template<>
struct SerializationTraits<bool> {
    static nlohmann::json Serialize(bool v) { return v; }
    static bool Deserialize(const nlohmann::json& j, bool fallback = false) { return j.is_boolean() ? j.get<bool>() : fallback; }
};

template<>
struct SerializationTraits<std::string> {
    static nlohmann::json Serialize(const std::string& v) { return v; }
    static std::string Deserialize(const nlohmann::json& j, const std::string& fallback = {}) {
        return j.is_string() ? j.get<std::string>() : fallback;
    }
};

template<>
struct SerializationTraits<math::Vec3> {
    static nlohmann::json Serialize(const math::Vec3& v) { return nlohmann::json::array({ v.x, v.y, v.z }); }
    static math::Vec3 Deserialize(const nlohmann::json& j, const math::Vec3& fallback = {}) {
        if (!j.is_array() || j.size() != 3) return fallback;
        return math::Vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    }
};

template<>
struct SerializationTraits<math::Quat> {
    static nlohmann::json Serialize(const math::Quat& q) { return nlohmann::json::array({ q.w, q.x, q.y, q.z }); }
    static math::Quat Deserialize(const nlohmann::json& j, const math::Quat& fallback = {}) {
        if (!j.is_array() || j.size() != 4) return fallback;
        return math::Quat(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
    }
};

template<>
struct SerializationTraits<math::Transform> {
    static nlohmann::json Serialize(const math::Transform& t) {
        nlohmann::json out;
        out["position"] = SerializationTraits<math::Vec3>::Serialize(t.position);
        out["rotation"] = SerializationTraits<math::Quat>::Serialize(t.rotation);
        out["scale"] = SerializationTraits<math::Vec3>::Serialize(t.scale);
        return out;
    }
    static math::Transform Deserialize(const nlohmann::json& j, const math::Transform& fallback = {}) {
        math::Transform t = fallback;
        if (!j.is_object()) return t;
        if (j.contains("position")) t.position = SerializationTraits<math::Vec3>::Deserialize(j["position"], t.position);
        if (j.contains("rotation")) t.rotation = SerializationTraits<math::Quat>::Deserialize(j["rotation"], t.rotation);
        if (j.contains("scale")) t.scale = SerializationTraits<math::Vec3>::Deserialize(j["scale"], t.scale);
        return t;
    }
};

template<typename T>
struct SerializationTraits<std::vector<T>> {
    static nlohmann::json Serialize(const std::vector<T>& v) {
        nlohmann::json out = nlohmann::json::array();
        for (const auto& item : v) out.push_back(SerializationTraits<T>::Serialize(item));
        return out;
    }
    static std::vector<T> Deserialize(const nlohmann::json& j) {
        std::vector<T> out;
        if (!j.is_array()) return out;
        out.reserve(j.size());
        for (const auto& item : j) out.push_back(SerializationTraits<T>::Deserialize(item));
        return out;
    }
};

} // namespace serialize
} // namespace eyelib
