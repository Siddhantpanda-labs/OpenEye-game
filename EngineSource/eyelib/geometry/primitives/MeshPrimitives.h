#pragma once

#include "eyelib/geometry/EditableMesh.h"

namespace eyelib {
namespace geometry {

/**
 * @brief MeshPrimitives: Generate primitive mesh shapes (CPU geometry).
 * 
 * Fills EditableMesh; caller syncs to GPU via UploadToGPU(device).
 */
class MeshPrimitives {
public:
    static void CreateCube(geometry::EditableMesh& editable, float size = 0.5f);
    static void CreateSphere(geometry::EditableMesh& editable, float radius = 0.5f, int segments = 32, int rings = 16);
    static void CreatePlane(geometry::EditableMesh& editable, float width = 1.0f, float height = 1.0f, 
                            int widthSegments = 1, int heightSegments = 1);
    static void CreateQuad(geometry::EditableMesh& editable);
    static void CreateCylinder(geometry::EditableMesh& editable, float radius = 0.5f, float height = 1.0f,
                               int segments = 32, int rings = 1, bool capTop = true, bool capBottom = true);
    static void CreateCone(geometry::EditableMesh& editable, float radius = 0.5f, float height = 1.0f,
                           int segments = 32, bool capBase = true);
    static void CreateTorus(geometry::EditableMesh& editable, float radius = 0.5f, float tubeRadius = 0.2f,
                            int segments = 32, int tubeSegments = 16);

private:
    static void FinalizeEditable(geometry::EditableMesh& editable);
};

} // namespace geometry
} // namespace eyelib
