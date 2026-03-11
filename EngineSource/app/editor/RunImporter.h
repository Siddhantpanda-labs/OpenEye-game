#pragma once

#include <string>

namespace openeye::editor {

/**
 * Run the AssetImporter tool to convert a source file (FBX/GLB/GLTF/etc.) to canonical assets.
 * Importer exe is expected next to the editor executable (AssetImporter.exe on Windows).
 * \param sourcePath Full path to the source file (e.g. .fbx, .glb).
 * \param outputDir  Directory where the importer will write (e.g. same folder as source).
 * \param separateMaterials If true, passes --mat (materials/*.mat); if false, materials embedded in .omesh.
 * \return true if importer ran and exited with 0; false if exe not found or non-zero exit.
 */
bool RunImporter(const std::string& sourcePath, const std::string& outputDir, bool separateMaterials);

} // namespace openeye::editor
