#include "api/geometry/Primitives.h"
#include "eyelib/geometry/primitives/MeshPrimitives.h"
#include "api/render/Mesh.h"

namespace eyelib {
namespace api {
namespace geometry {

std::shared_ptr<Mesh> Primitives::CreateCube(float size) {
    auto internalMesh = ::eyelib::geometry::MeshPrimitives::CreateCube(size);
    return Mesh::WrapInternal(internalMesh);
}

std::shared_ptr<Mesh> Primitives::CreateSphere(float radius, int segments, int rings) {
    auto internalMesh = ::eyelib::geometry::MeshPrimitives::CreateSphere(radius, segments, rings);
    return Mesh::WrapInternal(internalMesh);
}

std::shared_ptr<Mesh> Primitives::CreatePlane(float width, float height, int widthSegments, int heightSegments) {
    auto internalMesh = ::eyelib::geometry::MeshPrimitives::CreatePlane(width, height, widthSegments, heightSegments);
    return Mesh::WrapInternal(internalMesh);
}

std::shared_ptr<Mesh> Primitives::CreateQuad() {
    auto internalMesh = ::eyelib::geometry::MeshPrimitives::CreateQuad();
    return Mesh::WrapInternal(internalMesh);
}

std::shared_ptr<Mesh> Primitives::CreateCylinder(float radius, float height, int segments, int rings, bool capTop, bool capBottom) {
    auto internalMesh = ::eyelib::geometry::MeshPrimitives::CreateCylinder(radius, height, segments, rings, capTop, capBottom);
    return Mesh::WrapInternal(internalMesh);
}

std::shared_ptr<Mesh> Primitives::CreateCone(float radius, float height, int segments, bool capBase) {
    auto internalMesh = ::eyelib::geometry::MeshPrimitives::CreateCone(radius, height, segments, capBase);
    return Mesh::WrapInternal(internalMesh);
}

std::shared_ptr<Mesh> Primitives::CreateTorus(float radius, float tubeRadius, int segments, int tubeSegments) {
    auto internalMesh = ::eyelib::geometry::MeshPrimitives::CreateTorus(radius, tubeRadius, segments, tubeSegments);
    return Mesh::WrapInternal(internalMesh);
}

} // namespace geometry
} // namespace api
} // namespace eyelib
