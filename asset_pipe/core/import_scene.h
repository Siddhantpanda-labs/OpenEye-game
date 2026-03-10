#pragma once

#include "asset_pipe/core/import_mesh.h"
#include "asset_pipe/core/import_material.h"
#include "asset_pipe/core/import_texture.h"
#include "asset_pipe/core/import_animation.h"
#include "asset_pipe/core/import_node.h"

#include <vector>

namespace asset_pipe {

/** Unified intermediate scene (format-independent). */
struct ImportScene {
    std::vector<ImportMesh> meshes;
    std::vector<ImportMaterial> materials;
    std::vector<ImportTexture> textures;
    std::vector<ImportAnimation> animations;

    /** Root node index. Children referenced by ImportNode::children. */
    std::vector<ImportNode> nodes;
    int rootNodeIndex = -1;

    std::string sourcePath;  // original source file path
    std::string sourceName;  // base name for output naming (e.g. "car" from "car.fbx")
};

} // namespace asset_pipe
