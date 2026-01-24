#include "eyelib/render/resources/MeshOptimizer.h"
#include "eyelib/core/Log.h"

namespace eyelib {
namespace render {

void MeshOptimizer::OptimizeVertexCache(Mesh& mesh) {
    mesh.Optimize();
}

std::shared_ptr<Mesh> MeshOptimizer::GenerateLOD(const Mesh& mesh, size_t targetVertexCount) {
    (void)targetVertexCount;
    /* Stub: full simplification would use mesh simplification (e.g. quadric). */
    auto copy = std::make_shared<Mesh>();
    copy->SetVertices(mesh.GetVertices());
    if (mesh.GetIndexFormat() == IndexFormat::UInt32)
        copy->SetIndices(mesh.GetIndices32());
    else
        copy->SetIndices(mesh.GetIndices16());
    for (size_t i = 0; i < mesh.GetSubmeshCount(); ++i) {
        const auto& s = mesh.GetSubmesh(i);
        copy->AddSubmesh(s.indexOffset, s.indexCount, s.materialIndex);
    }
    copy->UploadToGPU();
    return copy;
}

} // namespace render
} // namespace eyelib
