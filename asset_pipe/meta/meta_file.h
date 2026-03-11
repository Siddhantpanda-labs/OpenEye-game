#pragma once

#include <string>
#include <vector>

namespace asset_pipe {

/**
 * In-memory representation of a .meta file.
 * Stored next to each asset (e.g. car.omesh.meta).
 * GUID is stored as string (e.g. "9f4c2b3e-8c12-49e1-a3d9-c45c2c6a7e11").
 */
struct MetaFile {
    std::string guid;
    std::string type;
    std::vector<std::string> dependencies;
    std::string importer;
    int version = 1;
};

/** Read a .meta file from disk. Returns false on I/O or parse error. */
bool ReadMetaFile(const std::string& path, MetaFile& out);

/** Write a .meta file (YAML-like format). Overwrites existing file. */
bool WriteMetaFile(const std::string& path, const MetaFile& meta);

/** Generate a new GUID string (dashed form, e.g. 9f4c2b3e-8c12-49e1-a3d9-c45c2c6a7e11). */
std::string GenerateGuidString();

} // namespace asset_pipe
