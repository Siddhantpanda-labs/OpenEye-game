#include "CollisionShapeFactory.h"
#include "../../rendering/resources/Mesh.h"
#include "../utils/PhysicsUtils.h"
#include "../../core/log/Log.h"
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btSphereShape.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>
#include <BulletCollision/CollisionShapes/btCylinderShape.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleIndexVertexArray.h>

namespace OpenEye {
namespace Physics {

// ============================================================================
// Primitive Shapes
// ============================================================================

std::shared_ptr<btCollisionShape> CollisionShapeFactory::CreateBox(const glm::vec3& halfExtents) {
    btVector3 btHalfExtents = Utils::PhysicsUtils::GlmToBullet(halfExtents);
    return std::shared_ptr<btCollisionShape>(new btBoxShape(btHalfExtents));
}

std::shared_ptr<btCollisionShape> CollisionShapeFactory::CreateSphere(float radius) {
    return std::shared_ptr<btCollisionShape>(new btSphereShape(radius));
}

std::shared_ptr<btCollisionShape> CollisionShapeFactory::CreateCapsule(float radius, float height) {
    return std::shared_ptr<btCollisionShape>(new btCapsuleShape(radius, height));
}

std::shared_ptr<btCollisionShape> CollisionShapeFactory::CreateCylinder(const glm::vec3& halfExtents) {
    btVector3 btHalfExtents = Utils::PhysicsUtils::GlmToBullet(halfExtents);
    return std::shared_ptr<btCollisionShape>(new btCylinderShape(btHalfExtents));
}

// ============================================================================
// Mesh-Based Shapes
// ============================================================================

std::shared_ptr<btCollisionShape> CollisionShapeFactory::CreateConvexHull(std::shared_ptr<Rendering::Mesh> mesh) {
    if (!mesh || mesh->GetVertices().empty()) {
        OE_WARN("CreateConvexHull: Invalid mesh provided");
        return nullptr;
    }
    
    auto shape = std::make_shared<btConvexHullShape>();
    const auto& vertices = mesh->GetVertices();
    
    for (const auto& vertex : vertices) {
        btVector3 point = Utils::PhysicsUtils::GlmToBullet(vertex.position);
        shape->addPoint(point);
    }
    
    // Optimize the shape
    shape->optimizeConvexHull();
    
    return shape;
}

std::shared_ptr<btCollisionShape> CollisionShapeFactory::CreateBvhTriangleMesh(std::shared_ptr<Rendering::Mesh> mesh) {
    if (!mesh || mesh->GetVertices().empty() || mesh->GetIndices().empty()) {
        OE_WARN("CreateBvhTriangleMesh: Invalid mesh provided");
        return nullptr;
    }
    
    const auto& vertices = mesh->GetVertices();
    const auto& indices = mesh->GetIndices();
    
    // Create vertex/index arrays for Bullet
    auto indexVertexArray = new btTriangleIndexVertexArray();
    
    // Vertex data
    btScalar* vertexBase = (btScalar*)vertices.data();
    int numVertices = static_cast<int>(vertices.size());
    int vertexStride = sizeof(Rendering::Vertex);
    
    // Index data
    const unsigned int* indexBase = indices.data();
    int numTriangles = static_cast<int>(indices.size() / 3);
    int indexStride = 3 * sizeof(unsigned int);
    
    // Add triangle data
    btIndexedMesh indexedMesh;
    indexedMesh.m_numTriangles = numTriangles;
    indexedMesh.m_triangleIndexBase = (const unsigned char*)indexBase;
    indexedMesh.m_triangleIndexStride = indexStride;
    indexedMesh.m_numVertices = numVertices;
    indexedMesh.m_vertexBase = (const unsigned char*)vertexBase;
    indexedMesh.m_vertexStride = vertexStride;
    indexedMesh.m_indexType = PHY_INTEGER;
    indexedMesh.m_vertexType = PHY_FLOAT;
    
    indexVertexArray->addIndexedMesh(indexedMesh);
    
    // Create BVH triangle mesh shape (optimized for static geometry)
    bool useQuantizedAabbCompression = true;
    auto shape = std::shared_ptr<btBvhTriangleMeshShape>(
        new btBvhTriangleMeshShape(indexVertexArray, useQuantizedAabbCompression)
    );
    
    return shape;
}

} // namespace Physics
} // namespace OpenEye

