#include "asset/writers/prefab_writer.h"
#include "asset/core/import_scene.h"
#include "asset/core/math_types.h"

#include <fstream>
#include <sstream>

namespace asset {

static void WriteNode(std::ostream& out, const ImportScene& scene, int nodeIndex, const Mat4& parentWorld, int depth) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(scene.nodes.size())) return;
    const ImportNode& node = scene.nodes[static_cast<size_t>(nodeIndex)];

    Mat4 world;
    for (int i = 0; i < 16; ++i)
        world.m[i] = parentWorld.m[i];  // TODO proper mat4 multiply; for stub we use local as world

    std::string indent(depth * 2, ' ');
    out << indent << "- name: \"" << node.name << "\"\n";
    out << indent << "  transform: [";
    for (int i = 0; i < 16; ++i) {
        if (i) out << ", ";
        out << node.localTransform.m[i];
    }
    out << "]\n";
    if (node.meshIndex >= 0 && node.meshIndex < static_cast<int>(scene.meshes.size())) {
        out << indent << "  mesh: \"" << scene.meshes[static_cast<size_t>(node.meshIndex)].name << ".mesh\"\n";
    }
    out << indent << "  children:\n";
    for (int childIdx : node.children) {
        WriteNode(out, scene, childIdx, world, depth + 1);
    }
}

bool PrefabWriter::Write(const ImportScene& scene, const std::string& path) {
    std::ofstream file(path);
    if (!file) return false;

    file << "prefab:\n";
    file << "  source: \"" << scene.sourcePath << "\"\n";
    file << "  entities:\n";

    Mat4 identity;
    if (scene.rootNodeIndex >= 0)
        WriteNode(file, scene, scene.rootNodeIndex, identity, 0);

    return file.good();
}

} // namespace asset
