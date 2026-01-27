#pragma once

#include "eyelib/render/resources/Mesh.h"
#include "eyelib/geometry/selection/Selection.h"
#include "eyelib/geometry/masking/Mask.h"
#include "eyelib/math/Types.h"
#include <memory>
#include <functional>

namespace eyelib {
namespace geometry {

/**
 * @brief MeshDisplace: Displacement operations.
 * 
 * Displaces vertices along normals or a custom direction using a scalar field.
 */
class MeshDisplace {
public:
    /**
     * @brief Displace vertices along their normals.
     * 
     * @param mesh Mesh to displace
     * @param selection Region to displace
     * @param amount Displacement amount (positive = outward, negative = inward)
     * @param mask Optional mask to control displacement strength per vertex
     */
    static void DisplaceAlongNormal(render::Mesh& mesh, const Selection& selection,
                                    float amount, const Mask* mask = nullptr);
    
    /**
     * @brief Displace vertices along a direction vector.
     * 
     * @param mesh Mesh to displace
     * @param selection Region to displace
     * @param direction Displacement direction (will be normalized)
     * @param amount Displacement amount
     * @param mask Optional mask to control displacement strength per vertex
     */
    static void DisplaceAlongDirection(render::Mesh& mesh, const Selection& selection,
                                      const math::Vec3& direction, float amount,
                                      const Mask* mask = nullptr);
    
    /**
     * @brief Displace vertices using a scalar field function.
     * 
     * @param mesh Mesh to displace
     * @param selection Region to displace
     * @param field Function that returns displacement amount for a position
     * @param direction Displacement direction (or use normals if nullptr)
     * @param mask Optional mask to control displacement strength per vertex
     */
    static void DisplaceWithField(render::Mesh& mesh, const Selection& selection,
                                  std::function<float(const math::Vec3&)> field,
                                  const math::Vec3* direction = nullptr,
                                  const Mask* mask = nullptr);
};

} // namespace geometry
} // namespace eyelib
