#pragma once

#include <string>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "entt/entt.hpp"
#include "rendering/resources/Mesh.h"
#include "rendering/resources/Material.h"
#include "rendering/Camera.h"
#include "rendering/LODManager.h"

namespace OpenEye {
namespace ECS {

// Transform component
struct Transform {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    
    // Parent entity (null entity if no parent)
    entt::entity parent = entt::null;
    
    // Cached world matrix (recomputed when dirty)
    mutable glm::mat4 worldMatrix = glm::mat4(1.0f);
    mutable bool dirty = true;
    
    glm::mat4 GetLocalMatrix() const {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
        transform = transform * glm::mat4_cast(rotation);
        transform = glm::scale(transform, scale);
        return transform;
    }
    
    glm::mat4 GetWorldMatrix(const entt::registry& registry) const;
};

// Name component
struct Name {
    std::string name;
    
    Name() = default;
    explicit Name(const std::string& n) : name(n) {}
};

// Renderable component
struct Renderable {
    std::shared_ptr<Rendering::Mesh> mesh;
    std::shared_ptr<Rendering::Material> material;
    bool visible = true;
    
    // LOD support: can have multiple meshes for different LOD levels
    std::vector<std::shared_ptr<Rendering::Mesh>> lodMeshes; // LOD 0, 1, 2, etc.
    size_t activeLOD = 0;
    
    Renderable() = default;
    Renderable(std::shared_ptr<Rendering::Mesh> m, std::shared_ptr<Rendering::Material> mat)
        : mesh(std::move(m)), material(std::move(mat)) {}
};

// Camera component
struct CameraComponent {
    Rendering::Camera camera;
    bool isPrimary = false; // Primary camera for rendering
    
    CameraComponent() = default;
};

// Light component
struct Light {
    enum class Type {
        Directional = 0,
        Point = 1,
        Spot = 2
    };
    
    Type type = Type::Directional;
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float range = 10.0f; // For point/spot lights
    float innerConeAngle = 15.0f; // For spot lights
    float outerConeAngle = 30.0f; // For spot lights
    bool castShadows = true;
};

// AABB component (for culling)
struct AABB {
    glm::vec3 min = glm::vec3(-0.5f);
    glm::vec3 max = glm::vec3(0.5f);
    bool isVisible = true; // Culling result flag
    
    glm::vec3 GetCenter() const {
        return (min + max) * 0.5f;
    }
    
    glm::vec3 GetExtents() const {
        return (max - min) * 0.5f;
    }
    
    glm::vec3 GetSize() const {
        return max - min;
    }
    
    // Transform AABB by matrix
    AABB Transform(const glm::mat4& matrix) const;
};

// LOD component (for level-of-detail management)
struct LOD {
    Rendering::LODSettings settings;
    
    LOD() = default;
    explicit LOD(const Rendering::LODSettings& s) : settings(s) {}
};

// Tag component (for grouping/querying)
struct Tag {
    std::vector<std::string> tags;
    
    void AddTag(const std::string& tag);
    void RemoveTag(const std::string& tag);
    bool HasTag(const std::string& tag) const;
};

} // namespace ECS
} // namespace OpenEye
