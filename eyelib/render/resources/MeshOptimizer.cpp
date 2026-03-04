#include "eyelib/render/resources/MeshOptimizer.h"
#include "eyelib/render/MeshDescriptor.h"
#include "eyelib/render/MeshDescriptorUtils.h"
#include "eyelib/core/Log.h"

namespace eyelib {
namespace render {

void MeshOptimizer::OptimizeVertexCache(MeshDescriptor& desc) {
    MeshDescriptorUtils::Optimize(desc);
}

std::shared_ptr<Mesh> MeshOptimizer::GenerateLOD(const MeshDescriptor& desc, size_t targetVertexCount, rhi::IDevice* device) {
    (void)targetVertexCount;
    /* Stub: full simplification would reduce desc.vertices/indices. */
    if (!device || desc.vertices.empty()) return nullptr;
    return Mesh::Create(device, desc);
}

} // namespace render
} // namespace eyelib
