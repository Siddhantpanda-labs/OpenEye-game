#include "SoftBodySystem.h"
#include "ecs/Scene.h"
#include "ecs/Components.h"
#include "physics/PhysicsWorld.h"
#include "rendering/resources/Mesh.h"
#include "rendering/Camera.h"
#include "rendering/resources/Shader.h"
#include "core/log/Log.h"
#include <BulletSoftBody/btSoftBody.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#include <algorithm>
#include <map>

using namespace OpenEye::Core;

namespace OpenEye {
namespace ECS {

// Static wireframe resources
GLuint SoftBodySystem::s_wireframeVAO = 0;
GLuint SoftBodySystem::s_wireframeVBO = 0;
std::shared_ptr<Rendering::Shader> SoftBodySystem::s_wireframeShader = nullptr;
bool SoftBodySystem::s_wireframeInitialized = false;

void SoftBodySystem::Update(Scene& scene, Physics::PhysicsWorld* physicsWorld, float deltaTime) {
    if (!physicsWorld) return;
    
    auto& registry = scene.GetRegistry();
    
    // Apply wind forces to soft bodies and keep fixed nodes pinned
    for (auto entity : registry.view<SoftBodyComponent, Transform>()) {
        auto& softBodyComp = registry.get<SoftBodyComponent>(entity);
        const auto& transform = registry.get<Transform>(entity);
        
        if (!softBodyComp.enabled || !softBodyComp.softBody) continue;
        
        btSoftBody* psb = softBodyComp.softBody;
        
        // Fixed nodes are now handled by Bullet's anchor system (attached to rigid body)
        // No need to manually reset positions - Bullet handles it
        
        // Apply wind if enabled
        if (softBodyComp.windEnabled && softBodyComp.windStrength > 0.0f) {
            ApplyWind(softBodyComp.softBody, softBodyComp.windDirection, softBodyComp.windStrength, deltaTime);
        }
    }
}

void SoftBodySystem::SyncMesh(Scene& scene) {
    auto& registry = scene.GetRegistry();
    
    // Sync soft body nodes to mesh vertices for rendering
    for (auto entity : registry.view<SoftBodyComponent, Renderable, Transform>()) {
        auto& softBodyComp = registry.get<SoftBodyComponent>(entity);
        auto& renderable = registry.get<Renderable>(entity);
        const auto& transform = registry.get<Transform>(entity);
        
        if (!softBodyComp.enabled || !softBodyComp.softBody || !renderable.mesh) continue;
        
        btSoftBody* psb = softBodyComp.softBody;
        auto& mesh = renderable.mesh;
        
        // Get mesh vertices
        auto& vertices = mesh->GetVertices();
        int nodeCount = psb->m_nodes.size();
        
        if (vertices.size() != nodeCount) {
            // Mesh vertex count doesn't match soft body node count
            // This can happen if mesh was created separately
            continue;
        }
        
        // Get transform position to convert world space nodes to local space
        glm::vec3 transformPos = transform.position;
        
        // Update vertex positions from soft body nodes (convert from world to local space)
        for (int i = 0; i < nodeCount && i < (int)vertices.size(); ++i) {
            const btVector3& nodePos = psb->m_nodes[i].m_x;
            const btVector3& nodeNormal = psb->m_nodes[i].m_n;
            // Convert world space node position to local space
            vertices[i].position = glm::vec3(nodePos.x(), nodePos.y(), nodePos.z()) - transformPos;
            vertices[i].normal = glm::vec3(nodeNormal.x(), nodeNormal.y(), nodeNormal.z());
        }
        
        // Mark mesh as dirty and upload to GPU
        mesh->MarkDirty();
        mesh->UploadToGPU();
    }
}

void SoftBodySystem::ApplyWind(btSoftBody* softBody, const glm::vec3& windDir, float windStrength, float deltaTime) {
    if (!softBody) return;

    // UI can temporarily set windDir near zero while dragging; avoid NaNs.
    glm::vec3 dir = windDir;
    const float len = glm::length(dir);
    if (len < 1e-4f) return;
    dir /= len;

    // Interpret windStrength as a "force scale" in world space.
    // Apply as FORCE (not directly into velocity) to avoid runaway drift and be stable with variable dt.
    // Small dt scaling keeps feel consistent if callers change update timestep.
    const float dt = (deltaTime > 0.0f ? deltaTime : 0.0f);
    btVector3 windForce(dir.x, dir.y, dir.z);
    windForce *= btScalar(windStrength);
    if (dt > 0.0f) {
        windForce *= btScalar(dt);
    }

    // Apply wind force to all dynamic nodes (skip anchored/fixed nodes: invMass == 0)
    for (int i = 0; i < softBody->m_nodes.size(); ++i) {
        btSoftBody::Node& node = softBody->m_nodes[i];
        if (node.m_im == btScalar(0)) continue;
        node.m_f += windForce;
    }
}

void SoftBodySystem::RenderWireframes(Scene& scene, const Rendering::Camera& camera, const glm::mat4& view, const glm::mat4& proj) {
    auto& registry = scene.GetRegistry();
    
    // Initialize wireframe resources if needed
    if (!s_wireframeInitialized) {
        CreateWireframeBox();
        s_wireframeInitialized = true;
    }
    
    if (!s_wireframeVAO || !s_wireframeShader) return;
    
    // Save current state
    GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    GLfloat lineWidth;
    glGetFloatv(GL_LINE_WIDTH, &lineWidth);
    
    // Set up for wireframe rendering
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glLineWidth(2.0f);
    
    s_wireframeShader->Bind();
    
    // Render wireframe boxes for all soft bodies
    for (auto entity : registry.view<SoftBodyComponent, Transform>()) {
        const auto& softBodyComp = registry.get<SoftBodyComponent>(entity);
        const auto& transform = registry.get<Transform>(entity);
        
        if (!softBodyComp.enabled || !softBodyComp.softBody) continue;
        
        btSoftBody* psb = softBodyComp.softBody;
        
        // Calculate AABB from soft body nodes
        if (psb->m_nodes.size() == 0) continue;
        
        glm::vec3 minPos(psb->m_nodes[0].m_x.x(), psb->m_nodes[0].m_x.y(), psb->m_nodes[0].m_x.z());
        glm::vec3 maxPos = minPos;
        
        for (int i = 1; i < psb->m_nodes.size(); ++i) {
            const btVector3& pos = psb->m_nodes[i].m_x;
            minPos.x = std::min(minPos.x, pos.x());
            minPos.y = std::min(minPos.y, pos.y());
            minPos.z = std::min(minPos.z, pos.z());
            maxPos.x = std::max(maxPos.x, pos.x());
            maxPos.y = std::max(maxPos.y, pos.y());
            maxPos.z = std::max(maxPos.z, pos.z());
        }
        
        // Convert to local space (subtract transform position)
        minPos -= transform.position;
        maxPos -= transform.position;
        
        // Calculate center and size
        glm::vec3 center = (minPos + maxPos) * 0.5f;
        glm::vec3 size = maxPos - minPos;
        
        // Create model matrix
        glm::mat4 model = transform.GetWorldMatrix(registry);
        model = glm::translate(model, center);
        model = glm::scale(model, size * 0.5f); // Scale to [-1,1] cube
        
        RenderWireframeBox(view, proj, model, size);
    }
    
    s_wireframeShader->Unbind();
    
    // Restore state
    glLineWidth(lineWidth);
    if (blendEnabled) glEnable(GL_BLEND);
    if (cullFaceEnabled) glEnable(GL_CULL_FACE);
    if (!depthTestEnabled) glDisable(GL_DEPTH_TEST);
}

void SoftBodySystem::CreateWireframeBox() {
    // Create wireframe box vertices (12 edges of a cube)
    // Cube is [-1, 1]^3 in local space
    static const float vertices[] = {
        // Bottom face
        -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,
        // Top face
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
        // Vertical edges
        -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f
    };
    
    glGenVertexArrays(1, &s_wireframeVAO);
    glGenBuffers(1, &s_wireframeVBO);
    glBindVertexArray(s_wireframeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, s_wireframeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    
    // Load wireframe shader
    s_wireframeShader = std::make_shared<Rendering::Shader>();
    s_wireframeShader->LoadFromFile("assets/shaders/debug/wireframe.vert",
                                    "assets/shaders/debug/wireframe.frag");
}

void SoftBodySystem::RenderWireframeBox(const glm::mat4& view, const glm::mat4& proj, const glm::mat4& model, const glm::vec3& size) {
    if (!s_wireframeVAO || !s_wireframeShader) return;
    
    s_wireframeShader->SetMat4("u_View", view);
    s_wireframeShader->SetMat4("u_Projection", proj);
    s_wireframeShader->SetMat4("u_Model", model);
    s_wireframeShader->SetVec3("u_Color", glm::vec3(0.0f, 1.0f, 0.0f)); // Green color
    
    glBindVertexArray(s_wireframeVAO);
    glDrawArrays(GL_LINES, 0, 24); // 12 edges * 2 vertices = 24 vertices
    glBindVertexArray(0);
}

} // namespace ECS
} // namespace OpenEye
