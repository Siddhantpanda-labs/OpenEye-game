#include "eyelib/geometry/operations/MeshNoise.h"
#include "eyelib/geometry/operations/MeshDisplace.h"

namespace eyelib {
namespace geometry {

void MeshNoise::ApplyNoise(assets::EditableMesh& editable, const Selection& selection,
                          std::function<float(const math::Vec3&)> noiseField,
                          float strength, const math::Vec3* direction,
                          const Mask* mask) {
    // Use displacement with noise field
    auto fieldFunc = [noiseField, strength](const math::Vec3& pos) -> float {
        return noiseField(pos) * strength;
    };
    
    MeshDisplace::DisplaceWithField(editable, selection, fieldFunc, direction, mask);
}

void MeshNoise::ApplyPerlinNoise(assets::EditableMesh& editable, const Selection& selection,
                                 float scale, float strength, uint32_t seed,
                                 const Mask* mask) {
    auto noiseField = NoiseField::Create(NoiseType::Perlin, scale, seed);
    ApplyNoise(editable, selection, noiseField, strength, nullptr, mask);
}

void MeshNoise::ApplyFBMNoise(assets::EditableMesh& editable, const Selection& selection,
                              float scale, float strength, int octaves,
                              float persistence, uint32_t seed, const Mask* mask) {
    auto noiseField = NoiseField::Create(NoiseType::FBM, scale, seed, octaves, persistence);
    ApplyNoise(editable, selection, noiseField, strength, nullptr, mask);
}

} // namespace geometry
} // namespace eyelib
