#include "eyelib/geometry/primitives/MeshPrimitives.h"
#include "eyelib/math/Types.h"
#include <cmath>
#include <algorithm>

namespace eyelib {
namespace geometry {

std::shared_ptr<render::Mesh> MeshPrimitives::CreateCube(float size) {
    auto mesh = std::make_shared<render::Mesh>();
    
    std::vector<render::Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // 8 vertices of a cube
    float s = size;
    vertices = {
        // Front face
        {{-s, -s,  s}, {0, 0, 1}, {0, 0}, {0, 0}, {1, 0, 0}, {1, 1, 1, 1}},
        {{ s, -s,  s}, {0, 0, 1}, {1, 0}, {0, 0}, {1, 0, 0}, {1, 1, 1, 1}},
        {{ s,  s,  s}, {0, 0, 1}, {1, 1}, {0, 0}, {1, 0, 0}, {1, 1, 1, 1}},
        {{-s,  s,  s}, {0, 0, 1}, {0, 1}, {0, 0}, {1, 0, 0}, {1, 1, 1, 1}},
        // Back face
        {{ s, -s, -s}, {0, 0, -1}, {0, 0}, {0, 0}, {-1, 0, 0}, {1, 1, 1, 1}},
        {{-s, -s, -s}, {0, 0, -1}, {1, 0}, {0, 0}, {-1, 0, 0}, {1, 1, 1, 1}},
        {{-s,  s, -s}, {0, 0, -1}, {1, 1}, {0, 0}, {-1, 0, 0}, {1, 1, 1, 1}},
        {{ s,  s, -s}, {0, 0, -1}, {0, 1}, {0, 0}, {-1, 0, 0}, {1, 1, 1, 1}},
        // Left face
        {{-s, -s, -s}, {-1, 0, 0}, {0, 0}, {0, 0}, {0, 0, -1}, {1, 1, 1, 1}},
        {{-s, -s,  s}, {-1, 0, 0}, {1, 0}, {0, 0}, {0, 0, -1}, {1, 1, 1, 1}},
        {{-s,  s,  s}, {-1, 0, 0}, {1, 1}, {0, 0}, {0, 0, -1}, {1, 1, 1, 1}},
        {{-s,  s, -s}, {-1, 0, 0}, {0, 1}, {0, 0}, {0, 0, -1}, {1, 1, 1, 1}},
        // Right face
        {{ s, -s,  s}, {1, 0, 0}, {0, 0}, {0, 0}, {0, 0, 1}, {1, 1, 1, 1}},
        {{ s, -s, -s}, {1, 0, 0}, {1, 0}, {0, 0}, {0, 0, 1}, {1, 1, 1, 1}},
        {{ s,  s, -s}, {1, 0, 0}, {1, 1}, {0, 0}, {0, 0, 1}, {1, 1, 1, 1}},
        {{ s,  s,  s}, {1, 0, 0}, {0, 1}, {0, 0}, {0, 0, 1}, {1, 1, 1, 1}},
        // Bottom face
        {{-s, -s, -s}, {0, -1, 0}, {0, 0}, {0, 0}, {1, 0, 0}, {1, 1, 1, 1}},
        {{ s, -s, -s}, {0, -1, 0}, {1, 0}, {0, 0}, {1, 0, 0}, {1, 1, 1, 1}},
        {{ s, -s,  s}, {0, -1, 0}, {1, 1}, {0, 0}, {1, 0, 0}, {1, 1, 1, 1}},
        {{-s, -s,  s}, {0, -1, 0}, {0, 1}, {0, 0}, {1, 0, 0}, {1, 1, 1, 1}},
        // Top face
        {{-s,  s,  s}, {0, 1, 0}, {0, 0}, {0, 0}, {1, 0, 0}, {1, 1, 1, 1}},
        {{ s,  s,  s}, {0, 1, 0}, {1, 0}, {0, 0}, {1, 0, 0}, {1, 1, 1, 1}},
        {{ s,  s, -s}, {0, 1, 0}, {1, 1}, {0, 0}, {1, 0, 0}, {1, 1, 1, 1}},
        {{-s,  s, -s}, {0, 1, 0}, {0, 1}, {0, 0}, {1, 0, 0}, {1, 1, 1, 1}}
    };
    
    indices = {
        // Front
        0, 1, 2, 2, 3, 0,
        // Back
        4, 5, 6, 6, 7, 4,
        // Left
        8, 9, 10, 10, 11, 8,
        // Right
        12, 13, 14, 14, 15, 12,
        // Bottom
        16, 17, 18, 18, 19, 16,
        // Top
        20, 21, 22, 22, 23, 20
    };
    
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    FinalizeMesh(mesh);
    return mesh;
}

std::shared_ptr<render::Mesh> MeshPrimitives::CreateSphere(float radius, int segments, int rings) {
    auto mesh = std::make_shared<render::Mesh>();
    
    segments = std::max(3, segments);
    rings = std::max(2, rings);
    
    std::vector<render::Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // Generate vertices
    // Create segments vertices per ring (not segments+1) to avoid duplicate seam vertices
    // The indices will wrap around using modulo to close the seam
    for (int y = 0; y <= rings; ++y) {
        float v = static_cast<float>(y) / static_cast<float>(rings);
        float phi = v * 3.14159265359f; // 0 to PI
        
        for (int x = 0; x < segments; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(segments);
            float theta = u * 2.0f * 3.14159265359f; // 0 to 2PI
            
            math::Vec3 pos;
            pos.x = radius * std::sin(phi) * std::cos(theta);
            pos.y = radius * std::cos(phi);
            pos.z = radius * std::sin(phi) * std::sin(theta);
            
            math::Vec3 normal = glm::normalize(pos);
            
            render::Vertex vert;
            vert.position = pos;
            vert.normal = normal;
            vert.uv = math::Vec2(u, v);
            vert.color = math::Vec4(1, 1, 1, 1);
            
            vertices.push_back(vert);
        }
    }
    
    // Generate indices
    // Connect quads around the sphere, using modulo to wrap around and close the seam
    for (int y = 0; y < rings; ++y) {
        for (int x = 0; x < segments; ++x) {
            int base = y * segments;
            int nextBase = (y + 1) * segments;
            
            // Use modulo to wrap around: when x+1 == segments, it wraps to 0
            int v0 = base + x;
            int v1 = base + ((x + 1) % segments);
            int v2 = nextBase + x;
            int v3 = nextBase + ((x + 1) % segments);
            
            // First triangle
            indices.push_back(v0);
            indices.push_back(v2);
            indices.push_back(v1);
            
            // Second triangle
            indices.push_back(v1);
            indices.push_back(v2);
            indices.push_back(v3);
        }
    }
    
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    FinalizeMesh(mesh);
    return mesh;
}

std::shared_ptr<render::Mesh> MeshPrimitives::CreatePlane(float width, float height, int widthSegments, int heightSegments) {
    auto mesh = std::make_shared<render::Mesh>();
    
    widthSegments = std::max(1, widthSegments);
    heightSegments = std::max(1, heightSegments);
    
    std::vector<render::Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float w = width * 0.5f;
    float h = height * 0.5f;
    
    // Generate vertices
    for (int y = 0; y <= heightSegments; ++y) {
        float v = static_cast<float>(y) / static_cast<float>(heightSegments);
        float py = -h + height * v;
        
        for (int x = 0; x <= widthSegments; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(widthSegments);
            float px = -w + width * u;
            
            render::Vertex vert;
            vert.position = math::Vec3(px, 0, py);
            vert.normal = math::Vec3(0, 1, 0);
            vert.uv = math::Vec2(u, v);
            vert.color = math::Vec4(1, 1, 1, 1);
            
            vertices.push_back(vert);
        }
    }
    
    // Generate indices
    for (int y = 0; y < heightSegments; ++y) {
        for (int x = 0; x < widthSegments; ++x) {
            int a = y * (widthSegments + 1) + x;
            int b = a + 1;
            int c = a + (widthSegments + 1);
            int d = c + 1;
            
            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(b);
            
            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);
        }
    }
    
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    FinalizeMesh(mesh);
    return mesh;
}

std::shared_ptr<render::Mesh> MeshPrimitives::CreateQuad() {
    return CreatePlane(1.0f, 1.0f, 1, 1);
}

std::shared_ptr<render::Mesh> MeshPrimitives::CreateCylinder(float radius, float height, int segments, int rings, bool capTop, bool capBottom) {
    auto mesh = std::make_shared<render::Mesh>();
    
    segments = std::max(3, segments);
    rings = std::max(1, rings);
    
    std::vector<render::Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float halfHeight = height * 0.5f;
    
    // Generate side vertices
    for (int y = 0; y <= rings; ++y) {
        float v = static_cast<float>(y) / static_cast<float>(rings);
        float py = -halfHeight + height * v;
        
        for (int x = 0; x < segments; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(segments);
            float theta = u * 2.0f * 3.14159265359f;
            
            float cosTheta = std::cos(theta);
            float sinTheta = std::sin(theta);
            
            render::Vertex vert;
            vert.position = math::Vec3(radius * cosTheta, py, radius * sinTheta);
            vert.normal = math::Vec3(cosTheta, 0, sinTheta);
            vert.uv = math::Vec2(u, v);
            vert.color = math::Vec4(1, 1, 1, 1);
            
            vertices.push_back(vert);
        }
    }
    
    // Generate side indices
    size_t sideVertexStart = 0;
    for (int y = 0; y < rings; ++y) {
        for (int x = 0; x < segments; ++x) {
            int a = sideVertexStart + y * segments + x;
            int b = a + 1;
            int c = a + segments;
            int d = c + 1;
            
            // Wrap around for seam
            if (x == segments - 1) {
                b = sideVertexStart + y * segments;
                d = sideVertexStart + (y + 1) * segments;
            }
            
            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(b);
            
            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);
        }
    }
    
    // Generate caps
    size_t capVertexStart = vertices.size();
    
    if (capBottom) {
        // Bottom center
        render::Vertex center;
        center.position = math::Vec3(0, -halfHeight, 0);
        center.normal = math::Vec3(0, -1, 0);
        center.uv = math::Vec2(0.5f, 0.5f);
        center.color = math::Vec4(1, 1, 1, 1);
        vertices.push_back(center);
        
        // Bottom rim vertices
        for (int x = 0; x < segments; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(segments);
            float theta = u * 2.0f * 3.14159265359f;
            float cosTheta = std::cos(theta);
            float sinTheta = std::sin(theta);
            
            render::Vertex vert;
            vert.position = math::Vec3(radius * cosTheta, -halfHeight, radius * sinTheta);
            vert.normal = math::Vec3(0, -1, 0);
            vert.uv = math::Vec2(cosTheta * 0.5f + 0.5f, sinTheta * 0.5f + 0.5f);
            vert.color = math::Vec4(1, 1, 1, 1);
            vertices.push_back(vert);
        }
        
        // Bottom cap indices
        uint32_t centerIdx = static_cast<uint32_t>(capVertexStart);
        for (int x = 0; x < segments; ++x) {
            uint32_t rim0 = static_cast<uint32_t>(capVertexStart + 1 + x);
            uint32_t rim1 = static_cast<uint32_t>(capVertexStart + 1 + ((x + 1) % segments));
            indices.push_back(centerIdx);
            indices.push_back(rim1);
            indices.push_back(rim0);
        }
    }
    
    if (capTop) {
        // Top center
        render::Vertex center;
        center.position = math::Vec3(0, halfHeight, 0);
        center.normal = math::Vec3(0, 1, 0);
        center.uv = math::Vec2(0.5f, 0.5f);
        center.color = math::Vec4(1, 1, 1, 1);
        vertices.push_back(center);
        
        // Top rim vertices
        for (int x = 0; x < segments; ++x) {
            float u = static_cast<float>(x) / static_cast<float>(segments);
            float theta = u * 2.0f * 3.14159265359f;
            float cosTheta = std::cos(theta);
            float sinTheta = std::sin(theta);
            
            render::Vertex vert;
            vert.position = math::Vec3(radius * cosTheta, halfHeight, radius * sinTheta);
            vert.normal = math::Vec3(0, 1, 0);
            vert.uv = math::Vec2(cosTheta * 0.5f + 0.5f, sinTheta * 0.5f + 0.5f);
            vert.color = math::Vec4(1, 1, 1, 1);
            vertices.push_back(vert);
        }
        
        // Top cap indices
        uint32_t centerIdx = static_cast<uint32_t>(vertices.size() - segments - 1);
        for (int x = 0; x < segments; ++x) {
            uint32_t rim0 = static_cast<uint32_t>(vertices.size() - segments + x);
            uint32_t rim1 = static_cast<uint32_t>(vertices.size() - segments + ((x + 1) % segments));
            indices.push_back(centerIdx);
            indices.push_back(rim0);
            indices.push_back(rim1);
        }
    }
    
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    FinalizeMesh(mesh);
    return mesh;
}

std::shared_ptr<render::Mesh> MeshPrimitives::CreateCone(float radius, float height, int segments, bool capBase) {
    auto mesh = std::make_shared<render::Mesh>();
    
    segments = std::max(3, segments);
    
    std::vector<render::Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float halfHeight = height * 0.5f;
    
    // Apex vertex
    render::Vertex apex;
    apex.position = math::Vec3(0, halfHeight, 0);
    apex.normal = math::Vec3(0, 1, 0);
    apex.uv = math::Vec2(0.5f, 1.0f);
    apex.color = math::Vec4(1, 1, 1, 1);
    vertices.push_back(apex);
    
    // Base rim vertices
    for (int x = 0; x < segments; ++x) {
        float u = static_cast<float>(x) / static_cast<float>(segments);
        float theta = u * 2.0f * 3.14159265359f;
        float cosTheta = std::cos(theta);
        float sinTheta = std::sin(theta);
        
        math::Vec3 pos(radius * cosTheta, -halfHeight, radius * sinTheta);
        math::Vec3 toApex = math::Vec3(0, halfHeight, 0) - pos;
        math::Vec3 side = math::Vec3(-sinTheta, 0, cosTheta);
        math::Vec3 normal = glm::normalize(glm::cross(side, toApex));
        
        render::Vertex vert;
        vert.position = pos;
        vert.normal = normal;
        vert.uv = math::Vec2(u, 0.0f);
        vert.color = math::Vec4(1, 1, 1, 1);
        vertices.push_back(vert);
    }
    
    // Side indices
    uint32_t apexIdx = 0;
    for (int x = 0; x < segments; ++x) {
        uint32_t base0 = 1 + x;
        uint32_t base1 = 1 + ((x + 1) % segments);
        indices.push_back(apexIdx);
        indices.push_back(base1);
        indices.push_back(base0);
    }
    
    // Base cap
    if (capBase) {
        // Base center
        render::Vertex center;
        center.position = math::Vec3(0, -halfHeight, 0);
        center.normal = math::Vec3(0, -1, 0);
        center.uv = math::Vec2(0.5f, 0.5f);
        center.color = math::Vec4(1, 1, 1, 1);
        vertices.push_back(center);
        
        uint32_t centerIdx = static_cast<uint32_t>(vertices.size() - 1);
        for (int x = 0; x < segments; ++x) {
            uint32_t rim0 = 1 + x;
            uint32_t rim1 = 1 + ((x + 1) % segments);
            indices.push_back(centerIdx);
            indices.push_back(rim0);
            indices.push_back(rim1);
        }
    }
    
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    FinalizeMesh(mesh);
    return mesh;
}

std::shared_ptr<render::Mesh> MeshPrimitives::CreateTorus(float radius, float tubeRadius, int segments, int tubeSegments) {
    auto mesh = std::make_shared<render::Mesh>();
    
    segments = std::max(3, segments);
    tubeSegments = std::max(3, tubeSegments);
    
    std::vector<render::Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // Generate vertices
    for (int s = 0; s <= segments; ++s) {
        float u = static_cast<float>(s) / static_cast<float>(segments);
        float theta = u * 2.0f * 3.14159265359f;
        
        math::Vec3 center(radius * std::cos(theta), 0, radius * std::sin(theta));
        math::Vec3 right(-std::sin(theta), 0, std::cos(theta));
        math::Vec3 up(0, 1, 0);
        
        for (int t = 0; t <= tubeSegments; ++t) {
            float v = static_cast<float>(t) / static_cast<float>(tubeSegments);
            float phi = v * 2.0f * 3.14159265359f;
            
            math::Vec3 offset = (right * std::cos(phi) + up * std::sin(phi)) * tubeRadius;
            math::Vec3 pos = center + offset;
            math::Vec3 normal = glm::normalize(offset);
            
            render::Vertex vert;
            vert.position = pos;
            vert.normal = normal;
            vert.uv = math::Vec2(u, v);
            vert.color = math::Vec4(1, 1, 1, 1);
            vertices.push_back(vert);
        }
    }
    
    // Generate indices
    for (int s = 0; s < segments; ++s) {
        for (int t = 0; t < tubeSegments; ++t) {
            int a = s * (tubeSegments + 1) + t;
            int b = a + 1;
            int c = a + (tubeSegments + 1);
            int d = c + 1;
            
            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(b);
            
            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);
        }
    }
    
    mesh->SetVertices(vertices);
    mesh->SetIndices(indices);
    FinalizeMesh(mesh);
    return mesh;
}

void MeshPrimitives::FinalizeMesh(std::shared_ptr<render::Mesh> mesh) {
    if (!mesh) return;
    
    // Generate normals if not already set
    mesh->GenerateNormals();
    
    // Generate tangents
    mesh->GenerateTangents();
    
    // Compute AABB
    mesh->ComputeAABB();
    
    // Upload to GPU
    mesh->UploadToGPU();
}

} // namespace geometry
} // namespace eyelib
