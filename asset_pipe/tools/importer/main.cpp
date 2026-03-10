/**
 * Asset pipeline importer: source file (FBX/GLTF/etc.) -> canonical .mesh, .material, .prefab.
 * Phase 1: no GUIDs; deterministic naming.
 */

#include "asset_pipe/tools/importer/SceneLoader.h"
#include "asset_pipe/tools/importer/SceneConverter.h"
#include <assimp/scene.h>
#include "asset_pipe/core/import_scene.h"
#include "asset_pipe/writers/mesh_writer.h"
#include "asset_pipe/writers/material_writer.h"
#include "asset_pipe/writers/prefab_writer.h"
#include "asset_pipe/writers/texture_writer.h"

#include <iostream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static void Log(const char* msg) {
    std::cout << "[Importer] " << msg << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << (argc ? argv[0] : "importer") << " <source_file> [output_dir]\n";
        std::cerr << "  source_file: FBX, GLTF, GLB, DAE, etc.\n";
        std::cerr << "  output_dir: default is directory of source_file\n";
        return 1;
    }

    const std::string sourcePath = argv[1];
    std::string outputDir = argc >= 3 ? argv[2] : fs::path(sourcePath).parent_path().string();
    if (outputDir.empty()) outputDir = ".";

    Log(("Loading " + sourcePath).c_str());

    asset_pipe::SceneLoader loader;
    const aiScene* scene = loader.Load(sourcePath.c_str());
    if (!scene) {
        std::cerr << "[Importer] Error: " << loader.GetError() << std::endl;
        return 2;
    }

    asset_pipe::ImportScene importScene;
    if (!asset_pipe::SceneConverter::Convert(scene, sourcePath.c_str(), importScene)) {
        std::cerr << "[Importer] Convert failed\n";
        return 3;
    }

    Log(("Meshes: " + std::to_string(importScene.meshes.size())).c_str());
    Log(("Materials: " + std::to_string(importScene.materials.size())).c_str());
    Log(("Textures: " + std::to_string(importScene.textures.size())).c_str());

    fs::create_directories(outputDir);
    fs::path out(outputDir);

    for (size_t i = 0; i < importScene.meshes.size(); ++i) {
        std::string meshPath = (out / (importScene.meshes[i].name + ".mesh")).string();
        if (asset_pipe::MeshWriter::Write(importScene.meshes[i], meshPath))
            Log(("Writing " + meshPath).c_str());
        else
            std::cerr << "[Importer] Failed to write " << meshPath << "\n";
    }

    for (size_t i = 0; i < importScene.textures.size(); ++i) {
        std::string texPath = (out / (importScene.textures[i].name + ".tex")).string();
        if (asset_pipe::TextureWriter::Write(importScene.textures[i], texPath))
            Log(("Writing " + texPath).c_str());
        else
            std::cerr << "[Importer] Failed to write " << texPath << "\n";
    }

    for (size_t i = 0; i < importScene.materials.size(); ++i) {
        std::string matPath = (out / (importScene.materials[i].name + ".material")).string();
        if (asset_pipe::MaterialWriter::Write(importScene.materials[i], matPath))
            Log(("Writing " + matPath).c_str());
        else
            std::cerr << "[Importer] Failed to write " << matPath << "\n";
    }

    std::string prefabPath = (out / (importScene.sourceName + ".prefab")).string();
    if (asset_pipe::PrefabWriter::Write(importScene, prefabPath))
        Log(("Writing " + prefabPath).c_str());
    else
        std::cerr << "[Importer] Failed to write " << prefabPath << "\n";

    Log("Done.");
    return 0;
}
