#pragma once

#include "asset_pipe/core/import_mesh.h"

#include <string>

namespace asset_pipe {

/** Writes ImportMesh to canonical .mesh binary. Deterministic output. */
class MeshWriter {
public:
    /** Returns true on success. Logs errors. */
    static bool Write(const ImportMesh& mesh, const std::string& path);
};

} // namespace asset_pipe
