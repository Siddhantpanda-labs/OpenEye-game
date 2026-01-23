#pragma once

#include "api/scene/Scene.h"

#include <string>

namespace eyelib {
namespace api {
namespace serialize {

class SceneJson {
public:
    static std::string ToString(const api::Scene& scene);
    static bool FromString(api::Scene& scene, const std::string& jsonText);

    static bool SaveToFile(const api::Scene& scene, const std::string& path);
    static bool LoadFromFile(api::Scene& scene, const std::string& path);
};

} // namespace serialize
} // namespace api
} // namespace eyelib

