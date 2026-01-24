#include "eyelib/physics/shapes/CollisionShapeFactory.h"
#include "eyelib/core/Log.h"

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btCylinderShape.h>
#include <BulletCollision/CollisionShapes/btConeShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>

#include <unordered_map>
#include <string>
#include <sstream>

namespace eyelib {
namespace physics {

namespace {
btVector3 ToBt(const math::Vec3& v) { return btVector3(v.x, v.y, v.z); }
btTransform ToBt(const math::Mat4& m) {
    btTransform t;
    t.setFromOpenGLMatrix(&m[0][0]);
    return t;
}

std::string BoxKey(const math::Vec3& h) {
    std::ostringstream o;
    o << "box:" << h.x << "," << h.y << "," << h.z;
    return o.str();
}
std::string SphereKey(float r) {
    std::ostringstream o;
    o << "sphere:" << r;
    return o.str();
}
std::string CapsuleKey(float r, float h) {
    std::ostringstream o;
    o << "capsule:" << r << "," << h;
    return o.str();
}
std::string CylinderKey(const math::Vec3& h) {
    std::ostringstream o;
    o << "cyl:" << h.x << "," << h.y << "," << h.z;
    return o.str();
}
std::string ConeKey(float r, float h) {
    std::ostringstream o;
    o << "cone:" << r << "," << h;
    return o.str();
}

struct ShapeEntry {
    std::shared_ptr<::btCollisionShape> shape;
    std::string cacheKey;  // non-empty if cached primitive
    std::vector<float> heightfieldData;  // keep alive for heightfield
    std::vector<math::Vec3> hullVertices;
    std::vector<math::Vec3> meshVertices;
    std::vector<uint32_t> meshIndices;
    std::unique_ptr<btTriangleIndexVertexArray> meshInterface;
};
static std::unordered_map<CollisionShapeHandle, ShapeEntry> s_shapes;
static std::unordered_map<std::string, CollisionShapeHandle> s_cache;
static CollisionShapeHandle s_nextHandle = 1;
} // namespace

CollisionShapeHandle CollisionShapeFactory::CreateBox(const math::Vec3& halfExtents) {
    std::string key = BoxKey(halfExtents);
    auto it = s_cache.find(key);
    if (it != s_cache.end()) return it->second;

    auto shape = std::make_shared<btBoxShape>(ToBt(halfExtents));
    CollisionShapeHandle h = s_nextHandle++;
    ShapeEntry e;
    e.shape = std::move(shape);
    e.cacheKey = key;
    s_shapes[h] = std::move(e);
    s_cache[key] = h;
    return h;
}

CollisionShapeHandle CollisionShapeFactory::CreateSphere(float radius) {
    std::string key = SphereKey(radius);
    auto it = s_cache.find(key);
    if (it != s_cache.end()) return it->second;

    auto shape = std::make_shared<btSphereShape>(radius);
    CollisionShapeHandle h = s_nextHandle++;
    ShapeEntry e;
    e.shape = std::move(shape);
    e.cacheKey = key;
    s_shapes[h] = std::move(e);
    s_cache[key] = h;
    return h;
}

CollisionShapeHandle CollisionShapeFactory::CreateCapsule(float radius, float height) {
    std::string key = CapsuleKey(radius, height);
    auto it = s_cache.find(key);
    if (it != s_cache.end()) return it->second;

    auto shape = std::make_shared<btCapsuleShape>(radius, height);
    CollisionShapeHandle h = s_nextHandle++;
    ShapeEntry e;
    e.shape = std::move(shape);
    e.cacheKey = key;
    s_shapes[h] = std::move(e);
    s_cache[key] = h;
    return h;
}

CollisionShapeHandle CollisionShapeFactory::CreateCylinder(const math::Vec3& halfExtents) {
    std::string key = CylinderKey(halfExtents);
    auto it = s_cache.find(key);
    if (it != s_cache.end()) return it->second;

    auto shape = std::make_shared<btCylinderShape>(ToBt(halfExtents));
    CollisionShapeHandle h = s_nextHandle++;
    ShapeEntry e;
    e.shape = std::move(shape);
    e.cacheKey = key;
    s_shapes[h] = std::move(e);
    s_cache[key] = h;
    return h;
}

CollisionShapeHandle CollisionShapeFactory::CreateCone(float radius, float height) {
    std::string key = ConeKey(radius, height);
    auto it = s_cache.find(key);
    if (it != s_cache.end()) return it->second;

    auto shape = std::make_shared<btConeShape>(radius, height);
    CollisionShapeHandle h = s_nextHandle++;
    ShapeEntry e;
    e.shape = std::move(shape);
    e.cacheKey = key;
    s_shapes[h] = std::move(e);
    s_cache[key] = h;
    return h;
}

CollisionShapeHandle CollisionShapeFactory::CreateConvexHull(const std::vector<math::Vec3>& vertices) {
    if (vertices.empty()) {
        core::Log::Warn("CollisionShapeFactory::CreateConvexHull: empty vertices");
        return 0;
    }
    auto hull = std::make_shared<btConvexHullShape>();
    for (const auto& v : vertices)
        hull->addPoint(ToBt(v));
    hull->optimizeConvexHull();

    std::vector<math::Vec3> stored(vertices.begin(), vertices.end());
    CollisionShapeHandle h = s_nextHandle++;
    ShapeEntry e;
    e.shape = std::shared_ptr<::btCollisionShape>(std::move(hull));
    e.hullVertices = std::move(stored);
    s_shapes[h] = std::move(e);
    return h;
}

CollisionShapeHandle CollisionShapeFactory::CreateTriangleMesh(
    const std::vector<math::Vec3>& vertices,
    const std::vector<uint32_t>& indices) {
    if (vertices.empty() || indices.size() < 3 || indices.size() % 3 != 0) {
        core::Log::Warn("CollisionShapeFactory::CreateTriangleMesh: invalid vertices/indices");
        return 0;
    }

    std::vector<math::Vec3> vcopy(vertices.begin(), vertices.end());
    std::vector<uint32_t> icopy(indices.begin(), indices.end());

    auto indexVertexArray = new btTriangleIndexVertexArray();
    btIndexedMesh mesh;
    mesh.m_numTriangles = static_cast<int>(icopy.size() / 3);
    mesh.m_triangleIndexBase = reinterpret_cast<const unsigned char*>(icopy.data());
    mesh.m_triangleIndexStride = 3 * sizeof(uint32_t);
    mesh.m_numVertices = static_cast<int>(vcopy.size());
    mesh.m_vertexBase = reinterpret_cast<const unsigned char*>(vcopy.data());
    mesh.m_vertexStride = sizeof(math::Vec3);
    mesh.m_indexType = PHY_INTEGER;
    mesh.m_vertexType = PHY_FLOAT;
    indexVertexArray->addIndexedMesh(mesh);

    auto shape = std::shared_ptr<::btCollisionShape>(
        new btBvhTriangleMeshShape(indexVertexArray, true));

    std::unique_ptr<btTriangleIndexVertexArray> iface(indexVertexArray);
    CollisionShapeHandle h = s_nextHandle++;
    ShapeEntry e;
    e.shape = std::move(shape);
    e.meshInterface = std::move(iface);
    e.meshVertices = std::move(vcopy);
    e.meshIndices = std::move(icopy);
    s_shapes[h] = std::move(e);
    return h;
}

CollisionShapeHandle CollisionShapeFactory::CreateHeightfield(
    int width, int length,
    const float* data,
    float minHeight, float maxHeight,
    int upAxis,
    bool flipQuadEdges) {
    if (!data || width < 2 || length < 2) {
        core::Log::Warn("CollisionShapeFactory::CreateHeightfield: invalid params");
        return 0;
    }
    ShapeEntry e;
    e.heightfieldData.assign(data, data + width * length);
    e.shape = std::shared_ptr<::btCollisionShape>(new btHeightfieldTerrainShape(
        width, length, e.heightfieldData.data(), minHeight, maxHeight, upAxis, flipQuadEdges));

    CollisionShapeHandle h = s_nextHandle++;
    s_shapes[h] = std::move(e);
    return h;
}

CollisionShapeHandle CollisionShapeFactory::CreateCompound(
    const std::vector<CollisionShapeHandle>& shapes,
    const std::vector<math::Mat4>& transforms) {
    if (shapes.size() != transforms.size() || shapes.empty()) {
        core::Log::Warn("CollisionShapeFactory::CreateCompound: shapes/transforms size mismatch or empty");
        return 0;
    }
    auto compound = std::make_shared<btCompoundShape>(true, 0);
    for (size_t i = 0; i < shapes.size(); ++i) {
        ::btCollisionShape* child = GetShape(shapes[i]);
        if (!child) continue;
        compound->addChildShape(ToBt(transforms[i]), child);
    }
    CollisionShapeHandle h = s_nextHandle++;
    ShapeEntry e;
    e.shape = std::shared_ptr<::btCollisionShape>(std::move(compound));
    s_shapes[h] = std::move(e);
    return h;
}

void CollisionShapeFactory::Destroy(CollisionShapeHandle handle) {
    auto it = s_shapes.find(handle);
    if (it == s_shapes.end()) return;
    if (!it->second.cacheKey.empty())
        s_cache.erase(it->second.cacheKey);
    s_shapes.erase(it);
}

bool CollisionShapeFactory::IsValid(CollisionShapeHandle handle) {
    return s_shapes.find(handle) != s_shapes.end();
}

::btCollisionShape* CollisionShapeFactory::GetShape(CollisionShapeHandle handle) {
    auto it = s_shapes.find(handle);
    if (it == s_shapes.end() || !it->second.shape) return nullptr;
    return it->second.shape.get();
}

} // namespace physics
} // namespace eyelib
