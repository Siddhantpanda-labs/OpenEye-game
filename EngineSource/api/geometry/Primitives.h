#pragma once

#include "api/render/RenderDevice.h"

#include <memory>

namespace eyelib {
namespace render { class Mesh; }
namespace api {
class Mesh; // Forward declaration for API Mesh
namespace geometry {

/**
 * @brief Primitives: Generate primitive mesh shapes (public API).
 * Requires a valid RenderDevice; builds geometry and uploads to GPU in one step.
 */
class Primitives {
public:
    static std::shared_ptr<Mesh> CreateCube(RenderDevice device, float size = 0.5f);
    static std::shared_ptr<Mesh> CreateSphere(RenderDevice device, float radius = 0.5f, int segments = 32, int rings = 16);
    static std::shared_ptr<Mesh> CreatePlane(RenderDevice device, float width = 1.0f, float height = 1.0f,
                                             int widthSegments = 1, int heightSegments = 1);
    static std::shared_ptr<Mesh> CreateQuad(RenderDevice device);
    static std::shared_ptr<Mesh> CreateCylinder(RenderDevice device, float radius = 0.5f, float height = 1.0f,
                                                 int segments = 32, int rings = 1,
                                                 bool capTop = true, bool capBottom = true);
    static std::shared_ptr<Mesh> CreateCone(RenderDevice device, float radius = 0.5f, float height = 1.0f,
                                            int segments = 32, bool capBase = true);
    static std::shared_ptr<Mesh> CreateTorus(RenderDevice device, float radius = 0.5f, float tubeRadius = 0.2f,
                                             int segments = 32, int tubeSegments = 16);
};

} // namespace geometry
} // namespace api
} // namespace eyelib
