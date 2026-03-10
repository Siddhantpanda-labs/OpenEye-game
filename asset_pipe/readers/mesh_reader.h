#pragma once

#include "asset_pipe/core/import_mesh.h"

#include <string>

namespace asset_pipe {

/** Reads canonical .mesh binary into ImportMesh. Returns true on success. */
class MeshReader {
public:
    static bool Read(const std::string& path, ImportMesh& out);
};

} // namespace asset_pipe
