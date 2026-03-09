#pragma once

#include "eyelib/assets/EditableMesh.h"

namespace eyelib {
namespace geometry {

/**
 * @brief MeshPrimitives: Generate primitive mesh shapes (CPU geometry).
 * 
 * Fills EditableMesh; caller syncs to GPU via UploadToGPU(device).
 */
class MeshPrimitives {
public:
    static void CreateCube(assets::EditableMesh& editable, float size = 0.5f);
    static void CreateSphere(assets::EditableMesh& editable, float radius = 0.5f, int segments = 32, int rings = 16);
    static void CreatePlane(assets::EditableMesh& editable, float width = 1.0f, float height = 1.0f, 
                            int widthSegments = 1, int heightSegments = 1);
    static void CreateQuad(assets::EditableMesh& editable);
    static void CreateCylinder(assets::EditableMesh& editable, float radius = 0.5f, float height = 1.0f,
                               int segments = 32, int rings = 1, bool capTop = true, bool capBottom = true);
    static void CreateCone(assets::EditableMesh& editable, float radius = 0.5f, float height = 1.0f,
                           int segments = 32, bool capBase = true);
    static void CreateTorus(assets::EditableMesh& editable, float radius = 0.5f, float tubeRadius = 0.2f,
                            int segments = 32, int tubeSegments = 16);

private:
    static void FinalizeEditable(assets::EditableMesh& editable);
};

} // namespace geometry
} // namespace eyelib
