#include "Components.h"
#include "Scene.h"
#include "entt/entt.hpp"
#include <algorithm>

namespace OpenEye {
namespace ECS {

glm::mat4 Transform::GetWorldMatrix(const entt::registry& registry) const {
    if (!dirty) {
        return worldMatrix;
    }
    
    glm::mat4 local = GetLocalMatrix();
    
    // If has parent, multiply by parent's world matrix
    if (parent != entt::null && registry.valid(parent)) {
        if (registry.all_of<Transform>(parent)) {
            const auto& parentTransform = registry.get<Transform>(parent);
            worldMatrix = parentTransform.GetWorldMatrix(registry) * local;
        } else {
            worldMatrix = local;
        }
    } else {
        worldMatrix = local;
    }
    
    dirty = false;
    return worldMatrix;
}

AABB AABB::Transform(const glm::mat4& matrix) const {
    // Transform all 8 corners and compute new AABB
    glm::vec3 corners[8] = {
        glm::vec3(min.x, min.y, min.z),
        glm::vec3(max.x, min.y, min.z),
        glm::vec3(min.x, max.y, min.z),
        glm::vec3(max.x, max.y, min.z),
        glm::vec3(min.x, min.y, max.z),
        glm::vec3(max.x, min.y, max.z),
        glm::vec3(min.x, max.y, max.z),
        glm::vec3(max.x, max.y, max.z)
    };
    
    glm::vec3 transformedMin = glm::vec3(matrix * glm::vec4(corners[0], 1.0f));
    glm::vec3 transformedMax = transformedMin;
    
    for (int i = 1; i < 8; i++) {
        glm::vec3 transformed = glm::vec3(matrix * glm::vec4(corners[i], 1.0f));
        transformedMin = glm::min(transformedMin, transformed);
        transformedMax = glm::max(transformedMax, transformed);
    }
    
    AABB result;
    result.min = transformedMin;
    result.max = transformedMax;
    return result;
}

void Tag::AddTag(const std::string& tag) {
    if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
        tags.push_back(tag);
    }
}

void Tag::RemoveTag(const std::string& tag) {
    tags.erase(std::remove(tags.begin(), tags.end(), tag), tags.end());
}

bool Tag::HasTag(const std::string& tag) const {
    return std::find(tags.begin(), tags.end(), tag) != tags.end();
}

} // namespace ECS
} // namespace OpenEye

