#pragma once

#include "asset/core/import_scene.h"

#include <string>

namespace asset {

/** Writes ImportScene node hierarchy to .prefab (entity list with transforms and mesh refs). */
class PrefabWriter {
public:
    static bool Write(const ImportScene& scene, const std::string& path);
};

} // namespace asset
