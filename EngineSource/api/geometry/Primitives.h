#pragma once

#include <memory>

namespace eyelib {
namespace render { class Mesh; }
namespace api {
class Mesh; // Forward declaration for API Mesh
namespace geometry {

/**
 * @brief Primitives: Generate primitive mesh shapes (public API).
 */
class Primitives {
public:
    static std::shared_ptr<Mesh> CreateCube(float size = 0.5f);
    static std::shared_ptr<Mesh> CreateSphere(float radius = 0.5f, int segments = 32, int rings = 16);
    static std::shared_ptr<Mesh> CreatePlane(float width = 1.0f, float height = 1.0f,
                                             int widthSegments = 1, int heightSegments = 1);
    static std::shared_ptr<Mesh> CreateQuad();
    static std::shared_ptr<Mesh> CreateCylinder(float radius = 0.5f, float height = 1.0f,
                                                 int segments = 32, int rings = 1,
                                                 bool capTop = true, bool capBottom = true);
    static std::shared_ptr<Mesh> CreateCone(float radius = 0.5f, float height = 1.0f,
                                            int segments = 32, bool capBase = true);
    static std::shared_ptr<Mesh> CreateTorus(float radius = 0.5f, float tubeRadius = 0.2f,
                                             int segments = 32, int tubeSegments = 16);
};

} // namespace geometry
} // namespace api
} // namespace eyelib
