#pragma once

#include "asset/core/import_mesh.h"

#include <string>

namespace asset {

/** Writes ImportMesh to canonical .mesh binary. Deterministic output. */
class MeshWriter {
public:
    /** Returns true on success. Logs errors. */
    static bool Write(const ImportMesh& mesh, const std::string& path);
};

} // namespace asset
