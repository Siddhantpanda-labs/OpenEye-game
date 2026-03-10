#pragma once

#include <string>

struct aiScene;

namespace asset_pipe {

struct ImportScene;

/** Fills ImportScene::textures from Assimp (embedded + file refs) and remaps material paths to canonical .tex names. */
void ExtractTextures(const aiScene* scene, const char* sourcePath, const std::string& sourceName, ImportScene& out);

} // namespace asset_pipe
