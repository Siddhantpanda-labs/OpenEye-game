#include "api/geometry/Primitives.h"
#include "eyelib/geometry/primitives/MeshPrimitives.h"
#include "api/render/Mesh.h"

namespace eyelib {
namespace api {
namespace geometry {

std::shared_ptr<Mesh> Primitives::CreateCube(RenderDevice device, float size) {
    if (!device) return nullptr;
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreateCube(*mesh->GetEditableMesh(), size);
    mesh->UploadToGPU(device);
    return mesh;
}

std::shared_ptr<Mesh> Primitives::CreateSphere(RenderDevice device, float radius, int segments, int rings) {
    if (!device) return nullptr;
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreateSphere(*mesh->GetEditableMesh(), radius, segments, rings);
    mesh->UploadToGPU(device);
    return mesh;
}

std::shared_ptr<Mesh> Primitives::CreatePlane(RenderDevice device, float width, float height, int widthSegments, int heightSegments) {
    if (!device) return nullptr;
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreatePlane(*mesh->GetEditableMesh(), width, height, widthSegments, heightSegments);
    mesh->UploadToGPU(device);
    return mesh;
}

std::shared_ptr<Mesh> Primitives::CreateQuad(RenderDevice device) {
    if (!device) return nullptr;
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreateQuad(*mesh->GetEditableMesh());
    mesh->UploadToGPU(device);
    return mesh;
}

std::shared_ptr<Mesh> Primitives::CreateCylinder(RenderDevice device, float radius, float height, int segments, int rings, bool capTop, bool capBottom) {
    if (!device) return nullptr;
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreateCylinder(*mesh->GetEditableMesh(), radius, height, segments, rings, capTop, capBottom);
    mesh->UploadToGPU(device);
    return mesh;
}

std::shared_ptr<Mesh> Primitives::CreateCone(RenderDevice device, float radius, float height, int segments, bool capBase) {
    if (!device) return nullptr;
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreateCone(*mesh->GetEditableMesh(), radius, height, segments, capBase);
    mesh->UploadToGPU(device);
    return mesh;
}

std::shared_ptr<Mesh> Primitives::CreateTorus(RenderDevice device, float radius, float tubeRadius, int segments, int tubeSegments) {
    if (!device) return nullptr;
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreateTorus(*mesh->GetEditableMesh(), radius, tubeRadius, segments, tubeSegments);
    mesh->UploadToGPU(device);
    return mesh;
}

} // namespace geometry
} // namespace api
} // namespace eyelib
