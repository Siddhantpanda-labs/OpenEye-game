#pragma once

struct aiScene;

namespace asset {

struct ImportScene;

/** Converts Assimp aiScene to engine ImportScene (format-independent). */
class SceneConverter {
public:
    /** Convert aiScene to out. Returns false on failure. */
    static bool Convert(const aiScene* scene, const char* sourcePath, ImportScene& out);
};

} // namespace asset
