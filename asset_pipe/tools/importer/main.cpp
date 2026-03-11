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
#include "asset_pipe/writers/animation_writer.h"
#include "asset_pipe/writers/oea_writer.h"
#include "asset_pipe/core/oea_format.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static void Log(const char* msg) {
    std::cout << "[Importer] " << msg << std::endl;
}

static bool WriteMaterialMap(const asset_pipe::ImportScene& importScene, const fs::path& outDir) {
    if (importScene.sourceName.empty()) return false;
    const fs::path mapPath = outDir / (importScene.sourceName + ".materialmap");
    std::ofstream file(mapPath, std::ios::binary);
    if (!file) return false;

    // Deterministic material index -> material file mapping for robust runtime lookup.
    file << "# material_index|material_file\n";
    for (size_t i = 0; i < importScene.materials.size(); ++i) {
        file << i << "|" << importScene.materials[i].name << ".material\n";
    }
    return file.good();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << (argc ? argv[0] : "importer") << " <source_file> [output_dir] [--keep-loose | --oea-only]\n";
        std::cerr << "  source_file: FBX, GLTF, GLB, DAE, etc.\n";
        std::cerr << "  output_dir: default is directory of source_file\n";
        std::cerr << "  default: writes .oea only\n";
        std::cerr << "  --keep-loose: keep loose sub-assets alongside .oea\n";
        std::cerr << "  --oea-only: alias for default behavior\n";
        return 1;
    }

    const std::string sourcePath = argv[1];
    std::string outputDir = fs::path(sourcePath).parent_path().string();
    bool keepLoose = false; // package-only by default
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--keep-loose") keepLoose = true;
        else if (arg == "--oea-only") keepLoose = false;
        else if (!arg.empty()) outputDir = arg;
    }
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
    Log(("Animations: " + std::to_string(importScene.animations.size())).c_str());

    fs::create_directories(outputDir);
    fs::path out(outputDir);
    fs::path writeDir = out;
    if (!keepLoose) {
        writeDir = fs::temp_directory_path() / "OpenEyeOeaStage" / importScene.sourceName;
        std::error_code ec;
        fs::remove_all(writeDir, ec);
        fs::create_directories(writeDir, ec);
        if (ec) {
            std::cerr << "[Importer] Failed to create staging directory: " << writeDir.string() << "\n";
            return 4;
        }
    }
    std::vector<std::string> writtenSubFiles;

    for (size_t i = 0; i < importScene.meshes.size(); ++i) {
        std::string meshPath = (writeDir / (importScene.meshes[i].name + ".mesh")).string();
        if (asset_pipe::MeshWriter::Write(importScene.meshes[i], meshPath)) {
            if (keepLoose) Log(("Writing " + meshPath).c_str());
            writtenSubFiles.push_back(meshPath);
        } else
            std::cerr << "[Importer] Failed to write " << meshPath << "\n";
    }

    for (size_t i = 0; i < importScene.textures.size(); ++i) {
        std::string texPath = (writeDir / (importScene.textures[i].name + ".tex")).string();
        if (asset_pipe::TextureWriter::Write(importScene.textures[i], texPath)) {
            if (keepLoose) Log(("Writing " + texPath).c_str());
            writtenSubFiles.push_back(texPath);
        } else
            std::cerr << "[Importer] Failed to write " << texPath << "\n";
    }

    for (size_t i = 0; i < importScene.materials.size(); ++i) {
        std::string matPath = (writeDir / (importScene.materials[i].name + ".material")).string();
        if (asset_pipe::MaterialWriter::Write(importScene.materials[i], matPath)) {
            if (keepLoose) Log(("Writing " + matPath).c_str());
            writtenSubFiles.push_back(matPath);
        } else
            std::cerr << "[Importer] Failed to write " << matPath << "\n";
    }

    if (WriteMaterialMap(importScene, writeDir)) {
        if (keepLoose) Log(("Writing " + (writeDir / (importScene.sourceName + ".materialmap")).string()).c_str());
        writtenSubFiles.push_back((writeDir / (importScene.sourceName + ".materialmap")).string());
    } else
        std::cerr << "[Importer] Failed to write material map for " << importScene.sourceName << "\n";

    for (size_t i = 0; i < importScene.animations.size(); ++i) {
        std::string animPath = (writeDir / (importScene.animations[i].name + ".anim")).string();
        if (asset_pipe::AnimationWriter::Write(importScene.animations[i], animPath)) {
            if (keepLoose) Log(("Writing " + animPath).c_str());
            writtenSubFiles.push_back(animPath);
        } else
            std::cerr << "[Importer] Failed to write " << animPath << "\n";
    }

    std::string prefabPath = (writeDir / (importScene.sourceName + ".prefab")).string();
    if (asset_pipe::PrefabWriter::Write(importScene, prefabPath)) {
        if (keepLoose) Log(("Writing " + prefabPath).c_str());
        writtenSubFiles.push_back(prefabPath);
    } else
        std::cerr << "[Importer] Failed to write " << prefabPath << "\n";

    std::vector<asset_pipe::OeaInputEntry> packageEntries;
    packageEntries.reserve(importScene.meshes.size() + importScene.textures.size() + importScene.materials.size() +
                           importScene.animations.size() + 2u);
    for (const auto& m : importScene.meshes)
        packageEntries.push_back({asset_pipe::OeaFormat::Entry_Mesh, m.name + ".mesh", (writeDir / (m.name + ".mesh")).string()});
    for (const auto& t : importScene.textures)
        packageEntries.push_back({asset_pipe::OeaFormat::Entry_Texture, t.name + ".tex", (writeDir / (t.name + ".tex")).string()});
    for (const auto& m : importScene.materials)
        packageEntries.push_back({asset_pipe::OeaFormat::Entry_Material, m.name + ".material", (writeDir / (m.name + ".material")).string()});
    for (const auto& a : importScene.animations)
        packageEntries.push_back({asset_pipe::OeaFormat::Entry_Animation, a.name + ".anim", (writeDir / (a.name + ".anim")).string()});
    const std::string materialMapPath = (writeDir / (importScene.sourceName + ".materialmap")).string();
    if (fs::exists(materialMapPath))
        packageEntries.push_back({asset_pipe::OeaFormat::Entry_MaterialMap, importScene.sourceName + ".materialmap", materialMapPath});
    packageEntries.push_back({asset_pipe::OeaFormat::Entry_Prefab, importScene.sourceName + ".prefab", prefabPath});

    const std::string oeaPath = (out / (importScene.sourceName + ".oea")).string();
    const bool oeaOk = asset_pipe::OeaWriter::Write(oeaPath, packageEntries);
    if (oeaOk)
        Log(("Writing " + oeaPath).c_str());
    else
        std::cerr << "[Importer] Failed to write " << oeaPath << "\n";

    if (!keepLoose && oeaOk) {
        std::error_code ec;
        fs::remove_all(writeDir, ec);
        if (ec) {
            std::cerr << "[Importer] Warning: failed to clean staging directory: " << writeDir.string() << "\n";
        }
        Log("Package-only mode: wrote .oea only.");
    }

    Log("Done.");
    return 0;
}
