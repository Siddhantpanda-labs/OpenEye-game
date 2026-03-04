#include "api/geometry/Primitives.h"
#include "eyelib/geometry/primitives/MeshPrimitives.h"
#include "eyelib/assets/AssetManager.h"
#include "api/render/Mesh.h"
#include "api/render/RenderDevice.h"

namespace eyelib {
namespace api {
namespace geometry {

static void UploadMeshToGPU(Mesh& mesh) {
    void* dev = ::eyelib::assets::AssetManager::Get().GetRenderDevice();
    if (dev) mesh.UploadToGPU(RenderDevice(dev));
}

std::shared_ptr<Mesh> Primitives::CreateCube(float size) {
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreateCube(*mesh->GetEditableMesh(), size);
    UploadMeshToGPU(*mesh);
    return mesh;
}

std::shared_ptr<Mesh> Primitives::CreateSphere(float radius, int segments, int rings) {
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreateSphere(*mesh->GetEditableMesh(), radius, segments, rings);
    UploadMeshToGPU(*mesh);
    return mesh;
}

std::shared_ptr<Mesh> Primitives::CreatePlane(float width, float height, int widthSegments, int heightSegments) {
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreatePlane(*mesh->GetEditableMesh(), width, height, widthSegments, heightSegments);
    UploadMeshToGPU(*mesh);
    return mesh;
}

std::shared_ptr<Mesh> Primitives::CreateQuad() {
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreateQuad(*mesh->GetEditableMesh());
    UploadMeshToGPU(*mesh);
    return mesh;
}

std::shared_ptr<Mesh> Primitives::CreateCylinder(float radius, float height, int segments, int rings, bool capTop, bool capBottom) {
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreateCylinder(*mesh->GetEditableMesh(), radius, height, segments, rings, capTop, capBottom);
    UploadMeshToGPU(*mesh);
    return mesh;
}

std::shared_ptr<Mesh> Primitives::CreateCone(float radius, float height, int segments, bool capBase) {
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreateCone(*mesh->GetEditableMesh(), radius, height, segments, capBase);
    UploadMeshToGPU(*mesh);
    return mesh;
}

std::shared_ptr<Mesh> Primitives::CreateTorus(float radius, float tubeRadius, int segments, int tubeSegments) {
    auto mesh = std::make_shared<Mesh>();
    ::eyelib::geometry::MeshPrimitives::CreateTorus(*mesh->GetEditableMesh(), radius, tubeRadius, segments, tubeSegments);
    UploadMeshToGPU(*mesh);
    return mesh;
}

} // namespace geometry
} // namespace api
} // namespace eyelib
