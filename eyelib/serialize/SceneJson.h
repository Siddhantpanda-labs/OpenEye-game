#pragma once

#include "eyelib/scene/Scene.h"

#include <string>

namespace eyelib {
namespace serialize {

class SceneJson {
public:
    // Returns JSON text (UTF-8).
    static std::string ToString(const scene::Scene& scene);

    // Clears and loads into scene. Returns false on parse/validation failure.
    static bool FromString(scene::Scene& scene, const std::string& jsonText);

    static bool SaveToFile(const scene::Scene& scene, const std::string& path);
    static bool LoadFromFile(scene::Scene& scene, const std::string& path);
};

} // namespace serialize
} // namespace eyelib

