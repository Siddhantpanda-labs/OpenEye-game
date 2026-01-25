#include "api/fluids/FluidSurfaceExtractor.h"

#include "eyelib/fluids/FluidSurfaceExtractor.h"

namespace eyelib {
namespace api {

class FluidSurfaceExtractor::Impl {
public:
    eyelib::fluids::FluidSurfaceExtractor extractor{};
};

FluidSurfaceExtractor::FluidSurfaceExtractor() : m_impl(new Impl()) {}
FluidSurfaceExtractor::~FluidSurfaceExtractor() = default;

std::shared_ptr<Mesh> FluidSurfaceExtractor::BuildMesh(const FluidSimulation& sim, float isoValue) const {
    auto internalMesh = m_impl->extractor.BuildMesh(*sim.GetInternal(), isoValue);
    return Mesh::WrapInternal(internalMesh);
}

} // namespace api
} // namespace eyelib
