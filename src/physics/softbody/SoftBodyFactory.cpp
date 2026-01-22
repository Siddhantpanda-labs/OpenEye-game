#include "SoftBodyFactory.h"
#include "physics/PhysicsWorld.h"
#include "core/log/Log.h"
#include <BulletSoftBody/btSoftBody.h>
#include <BulletSoftBody/btSoftBodyHelpers.h>
#include <btBulletDynamicsCommon.h>
#include <glm/gtc/type_ptr.hpp>

using namespace OpenEye::Core;

namespace OpenEye {
namespace Physics {

btSoftBody* SoftBodyFactory::CreateCloth(
    PhysicsWorld* world,
    const glm::vec3& position,
    int widthSegments,
    int heightSegments,
    float width,
    float height,
    float mass
) {
    if (!world) {
        OE_ERROR("SoftBodyFactory::CreateCloth: world is null");
        return nullptr;
    }
    
    btSoftBodyWorldInfo& worldInfo = world->GetSoftBodyWorldInfo();
    
    // Create cloth patch using Bullet helpers.
    // We want the cloth to HANG DOWN from the cylinder, so build the patch in a vertical plane.
    //
    // Top edge is at y = 0, bottom edge at y = -height, and the cloth spans X (width).
    // Note: CreatePatch creates nodes in row-major order starting from the (corner00->corner10) edge.
    btVector3 corner00(-width * 0.5f,  0.0f, 0.0f);       // top-left
    btVector3 corner10(+width * 0.5f,  0.0f, 0.0f);       // top-right
    btVector3 corner01(-width * 0.5f, -height, 0.0f);     // bottom-left
    btVector3 corner11(+width * 0.5f, -height, 0.0f);     // bottom-right
    
    // Create patch without fixed nodes initially
    btSoftBody* cloth = btSoftBodyHelpers::CreatePatch(
        worldInfo,
        corner00, corner10, corner01, corner11,
        widthSegments, heightSegments,
        0, true // no fixed nodes initially, generate masses
    );
    
    if (!cloth) {
        OE_ERROR("SoftBodyFactory::CreateCloth: Failed to create cloth");
        return nullptr;
    }
    
    // Don't fix nodes here - they will be attached to a cylinder rigid body
    // The top edge nodes will be anchored to the cylinder using appendAnchor
    
    // Configure cloth properties
    cloth->getCollisionShape()->setMargin(0.02f);
    cloth->generateBendingConstraints(2, cloth->appendMaterial());
    cloth->setTotalMass(mass);
    
    // Cloth configuration
    cloth->m_cfg.kDP = 0.01f; // Damping
    cloth->m_cfg.kDF = 0.1f;  // Dynamic friction
    cloth->m_cfg.kMT = 0.0f;  // Material thickness
    cloth->m_cfg.piterations = 10; // Position iterations
    cloth->m_cfg.diterations = 10; // Drift iterations
    cloth->m_cfg.citerations = 10; // Cluster iterations
    
    // Collision settings
    cloth->m_cfg.collisions = btSoftBody::fCollision::SDF_RD;
    cloth->m_cfg.collisions |= btSoftBody::fCollision::SDF_RDN;
    cloth->m_cfg.collisions |= btSoftBody::fCollision::SDF_RDF;
    cloth->setSelfCollision(false);
    
    // Set position
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(position.x, position.y, position.z));
    cloth->transform(transform);
    
    // Add to world
    world->AddSoftBody(cloth);
    
    OE_INFO_F("Created cloth: %dx%d segments, size %.2fx%.2f, mass %.2f", 
              widthSegments, heightSegments, width, height, mass);
    
    return cloth;
}

btSoftBody* SoftBodyFactory::CreateFromMesh(
    PhysicsWorld* world,
    std::shared_ptr<void> mesh, // Actually std::shared_ptr<Rendering::Mesh>
    const glm::vec3& position,
    float mass
) {
    if (!world || !mesh) {
        OE_ERROR("SoftBodyFactory::CreateFromMesh: world or mesh is null");
        return nullptr;
    }
    
    // TODO: Implement mesh-based soft body creation
    OE_WARN("SoftBodyFactory::CreateFromMesh: Not yet implemented");
    return nullptr;
}

btSoftBody* SoftBodyFactory::CreateRope(
    PhysicsWorld* world,
    const glm::vec3& startPos,
    const glm::vec3& endPos,
    int segments,
    float mass
) {
    if (!world) {
        OE_ERROR("SoftBodyFactory::CreateRope: world is null");
        return nullptr;
    }
    
    btSoftBodyWorldInfo& worldInfo = world->GetSoftBodyWorldInfo();
    
    btSoftBody* rope = btSoftBodyHelpers::CreateRope(
        worldInfo,
        btVector3(startPos.x, startPos.y, startPos.z),
        btVector3(endPos.x, endPos.y, endPos.z),
        segments,
        1 // Fixed at start
    );
    
    if (!rope) {
        OE_ERROR("SoftBodyFactory::CreateRope: Failed to create rope");
        return nullptr;
    }
    
    rope->setTotalMass(mass);
    rope->m_cfg.kDP = 0.1f; // Damping
    rope->m_cfg.piterations = 10;
    
    world->AddSoftBody(rope);
    
    OE_INFO_F("Created rope: %d segments, mass %.2f", segments, mass);
    
    return rope;
}

btSoftBody* SoftBodyFactory::CreateVolumeSphere(
    PhysicsWorld* world,
    const glm::vec3& position,
    float radius,
    int resolution,
    float mass,
    float pressure
) {
    if (!world) {
        OE_ERROR("SoftBodyFactory::CreateVolumeSphere: world is null");
        return nullptr;
    }
    if (radius <= 0.0f) {
        OE_ERROR("SoftBodyFactory::CreateVolumeSphere: radius must be > 0");
        return nullptr;
    }

    btSoftBodyWorldInfo& worldInfo = world->GetSoftBodyWorldInfo();

    btVector3 center(position.x, position.y, position.z);
    btVector3 radiusVec(radius, radius, radius);

    // Create an ellipsoid soft body (closed surface, good for a jelly-like ball)
    btSoftBody* body = btSoftBodyHelpers::CreateEllipsoid(worldInfo, center, radiusVec, resolution);
    if (!body) {
        OE_ERROR("SoftBodyFactory::CreateVolumeSphere: Failed to create ellipsoid");
        return nullptr;
    }

    // Configure simulation parameters for a stable "volume" softbody
    // Slightly larger margin helps soft bodies generate contacts against statics
    body->getCollisionShape()->setMargin(0.05f);

    // Mass + constraints
    body->setTotalMass(mass, true);
    body->generateBendingConstraints(2, body->appendMaterial());

    // Pressure / volume conservation (jelly / balloon feel)
    body->m_cfg.kPR = btScalar(pressure); // pressure coefficient
    body->m_cfg.kVC = btScalar(20.0f);    // volume conservation

    // Damping / friction
    body->m_cfg.kDP = btScalar(0.02f);
    body->m_cfg.kDF = btScalar(0.3f);
    body->m_cfg.kDG = btScalar(0.0f);

    // Solver iterations (more = stiffer/more stable)
    body->m_cfg.piterations = 15;
    body->m_cfg.viterations = 15;
    body->m_cfg.diterations = 10;
    body->m_cfg.citerations = 0;

    // Deformable solver pipeline expects deformable collision flags (SDF_RD*).
    body->m_cfg.collisions = btSoftBody::fCollision::SDF_RD;
    body->m_cfg.collisions |= btSoftBody::fCollision::SDF_RDN;
    body->m_cfg.collisions |= btSoftBody::fCollision::SDF_RDF;

    // Contact hardness (how strongly it resists penetration)
    body->m_cfg.kCHR = btScalar(1.0f); // rigid contacts hardness
    body->m_cfg.kKHR = btScalar(1.0f); // kinetic contacts hardness
    body->m_cfg.kSHR = btScalar(1.0f); // soft contacts hardness

    // Pose used for shape matching / volume
    body->setPose(true, false);

    // Deformable pipeline does not require cluster-vs-rigid mode; keep it off to avoid conflicting modes.

    world->AddSoftBody(body);

    OE_INFO_F("Created volume sphere: r=%.2f, mass=%.2f, pressure=%.2f, res=%d",
              radius, mass, pressure, resolution);

    return body;
}

void SoftBodyFactory::Destroy(PhysicsWorld* world, btSoftBody* softBody) {
    if (!world || !softBody) return;
    
    if (world->HasSoftBody(softBody)) {
        world->RemoveSoftBody(softBody);
    }
    
    delete softBody;
}

bool SoftBodyFactory::ExistsInWorld(PhysicsWorld* world, btSoftBody* softBody) {
    if (!world || !softBody) return false;
    return world->HasSoftBody(softBody);
}

void SoftBodyFactory::AttachClothNodesToRigidBody(
    btSoftBody* cloth,
    btRigidBody* rigidBody,
    const std::vector<int>& nodeIndices,
    bool disableCollision,
    float influence
) {
    if (!cloth || !rigidBody) {
        OE_ERROR("SoftBodyFactory::AttachClothNodesToRigidBody: Invalid parameters");
        return;
    }
    
    if (disableCollision) {
        // Match btSoftBody::appendAnchor behavior for collision disabling
        if (cloth->m_collisionDisabledObjects.findLinearSearch(rigidBody) == cloth->m_collisionDisabledObjects.size()) {
            cloth->m_collisionDisabledObjects.push_back(rigidBody);
        }
    }

    for (int nodeIndex : nodeIndices) {
        if (nodeIndex >= 0 && nodeIndex < cloth->m_nodes.size()) {
            // Use deformable anchor so it works correctly in the deformable pipeline.
            // (Classic appendAnchor is not enforced the same way by deformable constraints.)
            cloth->appendDeformableAnchor(nodeIndex, rigidBody);
        }
    }
    
    OE_INFO_F("Attached %zu cloth nodes to rigid body", nodeIndices.size());
}

} // namespace Physics
} // namespace OpenEye
